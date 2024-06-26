//
// Created by vedad on 21/08/18.
//

#include <stdint.h>
#include <stdio.h>
#include <algorithm>
#include <iostream>
#include <vector>
#include "ipasir.hh"
#include <sys/resource.h>
#include "FerpManager.h"

#define DEBUG 0
#define debugf(fmt, ...) \
            do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)


// taken from qrpcheck
static inline double read_cpu_time()
{
  struct rusage u;
  if (getrusage (RUSAGE_SELF, &u))
    return 0;
  return u.ru_utime.tv_sec + 1e-6 * u.ru_utime.tv_usec +
         u.ru_stime.tv_sec + 1e-6 * u.ru_stime.tv_usec;
}

FerpManager::~FerpManager()
{
  for(std::vector<Lit>* a : annotations)
    delete a;
  for(std::vector<Lit>* c : trace_clauses)
    delete c;
  for(std::array<uint32_t, 2>* a : antecedents)
    delete a;

  for (auto& outer_vec : original_clause_mapping) {
      for (auto& inner_vec : *outer_vec) {
          delete inner_vec;
      }
      delete outer_vec;
  }
  original_clause_mapping.clear();

  for (auto& outer_vec : nor_clauses) {
      delete outer_vec;
  }
  nor_clauses.clear();

  for (auto& pair : helper_variable_mapping) {
      delete pair.second;
  }
  helper_variable_mapping.clear();

#ifdef FERP_CERT
  if(aig != nullptr) delete aig;
#endif
}

int FerpManager::addVariables(const std::vector<Var>& prop, const std::vector<Var>& orig, const std::vector<Lit>& anno)
{
  std::vector<Lit>* anno_ptr = new std::vector<Lit>(anno);
  annotations.push_back(anno_ptr);
  
  bool success = true;
  for(uint32_t i = 0; success && i < prop.size(); i++)
    success &= prop_to_original.insert(std::pair<Var, Var>(prop[i], orig[i])).second;
  
  if(!success) return 1;
  
  for(uint32_t i = 0; i < prop.size(); i++)
    prop_to_annotation.insert(std::pair<Var, std::vector<Lit>*>(prop[i], anno_ptr));
  
  return 0;
}

int FerpManager::addClause(uint32_t id, std::vector<Lit>* clause, std::array<uint32_t, 2>* ante)
{
  if (!cnf_id_to_trace_id.insert(std::pair<uint32_t, uint32_t>(id, trace_clauses.size())).second) return 1;
  trace_id_to_cnf_id.push_back(id);
  
  if (!is_sat) {
    for(const Lit l : *clause)
      if(prop_to_original.find(var(l)) == prop_to_original.end()) return 2;
  }

  if(root != 0 && clause->empty()) return 3;
  
  if(clause->empty())
    root = (uint32_t)trace_clauses.size();
  
  std::sort(clause->begin(), clause->end(), lit_order);
  trace_clauses.push_back(clause);
  antecedents.push_back(ante);
  
  return 0;
}

bool FerpManager::isHelper(Var v)
{
  for (const auto &pair : prop_to_original)
  {
    auto key = pair.first;
    if (v == key) {
      return false;
    }
  }
  return true;
}

#ifdef FERP_CHECK
int FerpManager::check(const Formula& qbf)
{
  if (is_sat) {
    return checkSAT(qbf);
  } else {
    return checkUNSAT(qbf);
  }
}

int FerpManager::checkSAT(const Formula& qbf)
{
  sat_calls = 0;
  check_nor_time = 0;
  check_elimination_time = 0;
  check_sat_time = 0;
  check_resolution_time = 0;
  find_assignment_time = 0;
  eliminate_clauses_time = 0;

  uint32_t origin_idx = 0;
  for (auto clause : nor_clauses)
  {
    // clause comes from axiom rule
    int res = checkExpansionSAT(qbf, clause, origin_idx);
    if (res) return res;
    origin_idx += 1;
  }

  double start_check_resolution = read_cpu_time();
  for (auto i : res_clause_ids)
  {
    // clause comes from res rule
    int res = checkResolution(i);
    if (res) return res;
  }
  check_resolution_time = read_cpu_time() - start_check_resolution;

  return 0;
}

int FerpManager::checkUNSAT(const Formula& qbf)
{
  for(uint32_t i = 1; i < trace_clauses.size(); i++)
  {
    if(antecedents[i]->at(1) == 0)
    {
      // clause comes from axiom rule
      int res = checkExpansionUNSAT(qbf, i);
      if (res) return res;
    }
    else
    {
      // clause comes from res rule
      int res = checkResolution(i);
      if (res) return res;
    }
  }
  
  // check whether every clause is reachable
  int res = checkRedundant();
  if (res) return res;
  
  return 0;
}

int FerpManager::checkExpansionUNSAT(const Formula& qbf, uint32_t index)
{
  // check if original id exists and existential part size
  const std::vector<Lit>* prop_clause = trace_clauses[index];
  uint32_t orig_id = antecedents[index]->at(0) - 1;
  if(orig_id >= qbf.numClauses()) return 1;
  const Clause* qbf_clause = qbf.getClause(orig_id);
  if(qbf_clause->size_e != prop_clause->size()) return 2;
  
  // generate existential part of original clause
  orig_ex.clear();
  for(const Lit l : *prop_clause)
    orig_ex.push_back(make_lit(prop_to_original[var(l)], sign(l)));
  std::sort(orig_ex.begin(), orig_ex.end(), lit_order);
  
  // check if clauses are the same
  {
    const_lit_iterator li1 = qbf_clause->begin_e();
    auto li2 = orig_ex.begin();
    for(; li1 < qbf_clause->end_e() && li2 != orig_ex.end(); li1++, li2++)
      if(*li1 != *li2) return 3;
  }
  
  // todo: can be implemented much more efficiently
  // collect annotations of clause
  std::set<std::vector<Lit>*> added_anots;
  for(auto li = prop_clause->begin(); li != prop_clause->end(); li++)
    added_anots.insert(prop_to_annotation[var(*li)]);
  
  std::set<Lit> clause_anot;
  for(const std::vector<Lit>* a : added_anots)
    for(const Lit l : *a)
    {
      if(clause_anot.find(negate(l)) != clause_anot.end()) return 4;
      clause_anot.insert(l);
    }
  
  // check if the negated universals are in clause annotation
  for(const_lit_iterator li = qbf_clause->begin_a(); li < qbf_clause->end_a(); li++)
    if(clause_anot.find(negate(*li)) == clause_anot.end()) return 5;
  
  return 0;
}

int FerpManager::checkExpansionSAT(const Formula& qbf, std::vector<Lit>* prop_clause, uint32_t origin_idx)
{
  
  double start_check_nor_clause = read_cpu_time();

  std::vector<Lit> assignment;
  
  // const std::vector<Lit>* prop_clause = trace_clauses[index];
  auto orignal_clauses = original_clause_mapping[origin_idx];
  assert(prop_clause->size() == orignal_clauses->size());

  auto it1 = prop_clause->begin();
  auto it2 = orignal_clauses->begin();
  auto end1 = prop_clause->end();
  auto end2 = orignal_clauses->end();

  while (it1 != end1 && it2 != end2) {
    auto lit = *it1;
    auto origin_arr = *it2;

    // create literal array, if helper variable
    std::vector<Lit> literal_array;
    if (isHelper(var(lit))) {
      assert(sign(lit));
      for (auto l : *helper_variable_mapping[var(lit)]) {
        literal_array.push_back(l);
      }
    } else {
      literal_array.push_back(lit);
    }

    orig_ex.clear();    
    for (auto litt : literal_array) {      
      for (auto annotation : *prop_to_annotation[var(litt)]) {
        if (std::find(assignment.begin(), assignment.end(), annotation) == assignment.end()) {
          assignment.push_back(annotation);
        }
      }      
      orig_ex.push_back(make_lit(prop_to_original[var(litt)], sign(litt)));      
    }
    std::sort(orig_ex.begin(), orig_ex.end(), lit_order);

    for (auto origin : *origin_arr) {
      const Clause* qbf_clause = qbf.getClause(origin - 1);
      if(qbf_clause->size_a != literal_array.size()) return 2;

      // check if clauses are negated
      {
        const_lit_iterator li1 = qbf_clause->begin_a();
        auto li2 = orig_ex.begin();
        for(; li1 < qbf_clause->end_a() && li2 != orig_ex.end(); li1++, li2++)
          if(*li1 != -(*li2)) return 3;
      }
      
      // add all existentials negated to current assignment
      {
        for (auto ex_it = qbf_clause->begin_e(); ex_it < qbf_clause->end_e(); ex_it++) {
          if (std::find(assignment.begin(), assignment.end(), negate(*ex_it)) == assignment.end()) {
            assignment.push_back(negate(*ex_it));
          }
        }
      }
    }
    it1 += 1;
    it2 += 1;
  }
  check_nor_time += read_cpu_time() - start_check_nor_clause;
  
  double start_check_elimination = read_cpu_time();

  auto res = checkElimination(qbf, origin_idx, assignment);
  
  check_elimination_time += read_cpu_time() - start_check_elimination;
  if (res != 0) return res;

  return 0;
}

int FerpManager::checkElimination(const Formula& qbf, uint32_t origin_idx, std::vector<Lit> assignment)
{
  double start_find_assignment = read_cpu_time();

  // For each clause in \phi not referenced by the nor clause,
  auto orignal_clauses = original_clause_mapping[origin_idx];
  std::vector<bool> eliminated(qbf.numClauses(), false);
  for (auto origin_arr : *orignal_clauses) {
    for (auto original : *origin_arr) {
      eliminated[original - 1] = true;
    }
  }
  
  void *sat_solver = ipasir_init();
  // add the existential part of the clauses that needs to be eliminated
  for (unsigned i = 0; i < qbf.numClauses(); i++) {        
    if (eliminated[i]) {
      continue;
    }
    const Clause* qbf_clause = qbf.getClause(i);
    for (auto ex_it = qbf_clause->begin_e(); ex_it < qbf_clause->end_e(); ex_it++) {
      ipasir_add(sat_solver, *ex_it);
    }
    ipasir_add(sat_solver, 0);
  }
  
  // add the current assignment
  for (auto lit : assignment) {
    ipasir_add(sat_solver, lit);
    ipasir_add(sat_solver, 0);
  }

  double start_check_sat_time = read_cpu_time();

  auto is_sat = ipasir_solve(sat_solver) == 10;
  
  check_sat_time += (read_cpu_time() - start_check_sat_time);
  sat_calls += 1;

  if (!is_sat) {
    ipasir_release(sat_solver);
    return 102;
  }

  for(uint32_t qi = 0; qi < qbf.numQuants(); qi++)
  {
    const Quant* quant = qbf.getQuant(qi);
    if(quant->type == QuantType::EXISTS) {
      for(const_var_iterator vit = quant->begin(); vit != quant->end(); vit++) {
        assert(qbf.isExistential(*vit));
        Lit lit = ipasir_val(sat_solver, *vit);
        if (std::find(assignment.begin(), assignment.end(), lit) == assignment.end()) {
          assignment.push_back(lit);
        }
      }
    }
  }
  ipasir_release(sat_solver);

  find_assignment_time += (read_cpu_time() - start_find_assignment);


  double start_eliminate_clauses = read_cpu_time();

  // Check that the assignment array does not contain a literal and its negated literal.
  for (auto lit : assignment) {
    if (std::find(assignment.begin(), assignment.end(), negate(lit)) != assignment.end()) {
      return 101;
    }
  }

  // check if assignment eliminates the remaining clauses
  for (unsigned i = 0; i < qbf.numClauses(); i++) {
    if (eliminated[i]) {
      continue;
    }
    const Clause* qbf_clause = qbf.getClause(i);
    for (auto lit : assignment) {
      if (std::find(qbf_clause->begin_e(), qbf_clause->end_e(), lit) != qbf_clause->end_e()) {
        eliminated[i] = true;
        break;
      }
    }
  }
  auto all_eliminated = std::all_of(eliminated.begin(), eliminated.end(), [](bool b) { return b; });
  if (!all_eliminated) {
    return 103;
  }

  eliminate_clauses_time += (read_cpu_time() - start_eliminate_clauses);
  
  return 0;
}

int FerpManager::checkResolution(uint32_t index)
{
  const std::vector<Lit>* prop_clause = trace_clauses[index];
  const std::vector<Lit>* _parent1 = trace_clauses[cnf_id_to_trace_id[antecedents[index]->at(0)]];
  const std::vector<Lit>* _parent2 = trace_clauses[cnf_id_to_trace_id[antecedents[index]->at(1)]];

  std::vector<Lit> parent1;
  for (auto lit : *_parent1) {
    if (std::find(parent1.begin(), parent1.end(), lit) == parent1.end())
      parent1.push_back(lit);
  }
  std::vector<Lit> parent2;
  for (auto lit : *_parent2) {
    if (std::find(parent2.begin(), parent2.end(), lit) == parent2.end())
      parent2.push_back(lit);
  }
  
  // find the pivot and check resolvent at the same time
  std::vector<Var> pivot_candidates;
  auto li1 = parent1.begin();
  auto li2 = parent2.begin();
  auto res = prop_clause->begin();
  
  while(li1 != parent1.end() && li2 != parent2.end())
  {
    const Var v1 = var(*li1);
    const Var v2 = var(*li2);
    if(v1 < v2)
    {
      if(*li1 != *res) return 6;
      
      li1++; res++;
    }
    else if(v1 > v2)
    {
      if(*li2 != *res) return 7;
      
      li2++; res++;
    }
    else
    {
      if(*li1 != *li2)
        pivot_candidates.push_back(v1);
      else if(*li1 == *res)
        res++;
      else
        return 8;
      
      li1++; li2++;
    }
  }
  
  // check if the rest of the elements are present
  while(li1 != parent1.end())
  {
    if(*li1 != *res) return 9;
    li1++; res++;
  }
  
  while(li2 != parent2.end())
  {
    if(*li2 != *res) return 10;
    li2++; res++;
  }
  
  // check that no additional elements are there
  if(res != prop_clause->end()) return 11;
  // check if clause not tautological
  if(pivot_candidates.size() != 1) return 12;
  
  return 0;
}

int FerpManager::checkRedundant()
{
  // check if root exists
  if(root == 0) return 13;
  
  std::vector<bool> mark(trace_clauses.size(), false);
  std::vector<uint32_t> queue;
  queue.push_back(root);
  mark[root] = true;
  
  // todo: cycle detection
  
  while(!queue.empty())
  {
    uint32_t node = queue.back();
    queue.pop_back();
    
    uint32_t next = antecedents[node]->at(1);
    if(next == 0) continue;
    next = cnf_id_to_trace_id[next];
    if(!mark[next])
    {
      mark[next] = true;
      queue.push_back(next);
    }
    
    next = cnf_id_to_trace_id[antecedents[node]->at(0)];
    
    if(!mark[next])
    {
      mark[next] = true;
      queue.push_back(next);
    }
  }
  
  for(uint32_t i = 1; i < mark.size(); i++)
    if(!mark[i]) return 14;
  
  return 0;
}
#endif // FERP_CHECK

#ifdef FERP_CERT

#define make_aiger_lit(v, s) \
  (aiger_var2lit(v) | !!s)

int FerpManager::extract(const Formula& qbf)
{
  collectPivots();
  collectIndicators();
  
  if(aig != nullptr) aiger_reset(aig);
  
  aig = aiger_init();
  
  // add all inputs from the existential variables
  for(uint32_t qi = 0; qi < qbf.numQuants(); qi++)
  {
    const Quant* quant = qbf.getQuant(qi);
    if(quant->type == QuantType::EXISTS)
      for(const_var_iterator vit = quant->begin(); vit != quant->end(); vit++)
        aiger_add_input(aig, aiger_var2lit(*vit), nullptr);
  }
  
  uint32_t num_prop = prop_to_original.rbegin()->first + 1;
  
  current_aig_var = (uint32_t)(qbf.numVars() + 1);
  
  std::vector<std::vector<uint32_t>> active(trace_clauses.size(), std::vector<uint32_t>(num_prop, aiger_false));
  std::vector<std::vector<uint32_t>> cumulative(trace_clauses.size(), std::vector<uint32_t>(num_prop, aiger_false));

  std::vector<uint32_t> top(trace_clauses.size(), aiger_false);
  std::vector<bool> mark;
  mark.clear(); mark.resize(trace_clauses.size(), false);

  // prepare active, cumulative data structures
  {
    // handle leaves
    for(uint32_t ci = 1; ci < trace_clauses.size(); ci++)
    {
      if(pivots[ci] != 0) continue;

      mark[ci] = true;
      const std::vector<Lit>* clause = trace_clauses[ci];
      for(uint32_t li = 0; li < clause->size(); li++)
      {
        const Var prop_v = var(clause->at(li));
        active[ci][prop_v] = aiger_true;
        cumulative[ci][prop_v] = aiger_true;
      }
    }
    // handle nodes in topological order
    bool updated = true;
    while(updated)
    {
      updated = false;

      for(uint32_t ci = 1; ci < trace_clauses.size(); ci++)
      {
        if(mark[ci]) continue;

        uint32_t parent1 = antecedents[ci]->at(0);
        uint32_t parent2 = antecedents[ci]->at(1);
        Var pivot = pivots[ci];
        assert(pivot != 0);

        if(!mark[parent1] || !mark[parent2]) continue;

        mark[ci] = true;
        updated = true;

        const std::vector<Lit>* clause = trace_clauses[ci];

        for(uint32_t li = 0; li < clause->size(); li++)
        {
          const Var prop_v = var(clause->at(li));
          active[ci][prop_v] = aiger_true;
        }

        for(uint32_t vi = 1; vi < num_prop; vi++)
        {
          cumulative[ci][vi] = makeOR(cumulative[parent1][vi], cumulative[parent2][vi]);
        }
      }
    }
  }

  // special handling for universal variables which are in the first block
  uint32_t qinit = 0;
  const Quant* quant0 = qbf.getQuant(0);
  if(quant0->type == QuantType::FORALL)
  {
    for(const_var_iterator vit = quant0->begin(); vit != quant0->end(); vit++)
    {
      Lit l_pos = make_lit(*vit, false);
      Lit l_neg = make_lit(*vit, true);
    
      auto lpiter = indicators.find(l_pos);
      auto lniter = indicators.find(l_neg);
      
      assert(lniter != indicators.end() && lpiter == indicators.end() ||
             lpiter != indicators.end() && lniter == indicators.end());
      
      uint32_t ind = (lniter == indicators.end()) ? aiger_true : aiger_false;
      
      aiger_add_and(aig, aiger_var2lit(*vit), ind, ind);
      aiger_add_output(aig, aiger_var2lit(*vit), nullptr);
      triv_out.insert(aiger_var2lit(*vit));
    }
    qinit = 1;
  }
  
  for(uint32_t qi = qinit; qi < qbf.numQuants() - 1; qi++)
  {
    const Quant* quant = qbf.getQuant(qi);
    if(quant->type == QuantType::EXISTS) continue;
    
    // reset marked clauses
    mark.clear(); mark.resize(trace_clauses.size(), false);
    
    // apply active, cumulative, and top rule for leaf clauses
    // * top rule:  clause is satisfied if the partial clause containing literals from a previous layer is satisfied
    // * active rule : variable is active if it is not assigned and contained in the clause and clause is not satisfied
    // * cumulative rule : same as active for leaf clauses
    ////   example qbf  e 1 2 3 a 4 5 e 6 7 . (-1 | 3 | 5 | 7) & ...
    ////   proof leaf: (-1 | 3 | 7^{4 -5})    top: (-1 | 3)
    ////                                   active:  1 : false, 2 : false, 3: false, 6^{4 -5}: false, 7^{4 -5}: (1 & -3)
    ////                               cumulative:  1 : false, 2 : false, 3: false, 6^{4 -5}: false, 7^{4 -5}: (1 & -3)
    for(uint32_t ci = 1; ci < trace_clauses.size(); ci++)
    {
      if(pivots[ci] != 0) continue;
      
      mark[ci] = true;
      const std::vector<Lit>* clause = trace_clauses[ci];
      
      // generate cube of negated literals in previous existential quantifier
      uint32_t out = aiger_not(top[ci]); // continue from previously computed cube
      for(uint32_t li = 0; li < clause->size(); li++)
      {
        const Lit l = clause->at(li);
        // ignore literals not in previous existential layer
        if(qbf.getVarDepth(prop_to_original[var(l)]) != (qi - 1)) continue;
        
        uint32_t rhs = make_aiger_lit(prop_to_original[var(l)], !sign(l));
        
        out = makeAND(out, rhs);
      }
      
      // the clause is satisfied by assignment if the part of the clause
      // containing literals from previous existential quantifiers is satisfied
      top[ci] = aiger_not(out);
      debugf("top value for %d : %d\n", ci, top[ci]);
      // active/cumulative if not assigned, and clause is not satisfied by assignment
      for(uint32_t li = 0; li < clause->size(); li++)
      {
        const Var prop_v = var(clause->at(li));
        uint32_t a = (qbf.getVarDepth(prop_to_original[prop_v]) < qi) ? aiger_false : out;

        debugf("active, cumulative for %d_%d : %d\n", prop_v, ci, a);

        active[ci][prop_v] = a;
        cumulative[ci][prop_v] = a;
      }
    }
    
    // go through the rest of the proof in some topological order: antecedents always come before the resolvent
    // * top rule: a clause is true if the pivot is not active in both parents and
    //                                 parent 1 is true or contains the pivot and
    //                                 parent 2 is true or contains the pivot
    // * active rule: if the variable is the pivot then it cannot be active
    //                if both parents have active pivot then each variable is active if it is active in either parent
    //                if parent 1 is not true and does not have active pivot: inherit activity from parent1
    //                if parent 2 is not true and does not have active pivot: inherit activity from parent2
    //                else the clause is true and nothing is active there anymore
    // * cumulative rule: same as active rule, but pivots don't get any special treatment
    bool updated = true;
    while(updated)
    {
      updated = false;
      
      for(uint32_t ci = 1; ci < trace_clauses.size(); ci++)
      {
        if(mark[ci]) continue;
        
        uint32_t parent1 = antecedents[ci]->at(0);
        uint32_t parent2 = antecedents[ci]->at(1);
        Var pivot = pivots[ci];
        assert(pivot != 0);
        
        if(!mark[parent1] || !mark[parent2]) continue;
        
        mark[ci] = true;
        updated = true;
        
        // pivot is active in both parents, resolution is possible
        uint32_t cond1 = makeAND(active[parent1][pivot], active[parent2][pivot]);
        // parent 1 is not true and does not have active pivot
        uint32_t cond2 = makeAND(aiger_not(active[parent1][pivot]), aiger_not(top[parent1]));
        // parent 2 is not true and does not have active pivot
        uint32_t cond3 = makeAND(aiger_not(active[parent2][pivot]), aiger_not(top[parent2]));

        debugf("conditions: %d %d %d\n", cond1, cond2, cond3);

        // clause is set to true if none of the conditions apply
        top[ci] = makeOR(top[ci], makeAND(makeAND(aiger_not(cond1), aiger_not(cond2)), aiger_not(cond3)));

        debugf("top value for %d : %d\n", ci, top[ci]);

        for(uint32_t vi = 1; vi < num_prop; vi++)
        {
          if(vi == pivot) // pivot cannot be active in the resolvent
          {
            active[ci][vi] = aiger_false;
            continue;
          }
          
          // optimisation: condition 3 is true and variable is active in parent 2
          uint32_t else_branch = makeAND(cond3, active[parent2][vi]);
          // active in either parent
          uint32_t resolv = makeOR(active[parent1][vi], active[parent2][vi]);
          // big if then else for deciding from where the activity comes
          uint32_t interm = makeITE(cond2, active[parent1][vi], else_branch);
          debugf("%d = if %d then %d else %d\n", interm, cond2, active[parent1][vi], else_branch);
          active[ci][vi] = makeAND(active[ci][vi], makeITE(cond1, resolv, interm));
          debugf("%d = if %d then %d else %d\n", active[ci][vi], cond1, resolv, interm);
          debugf("active for %d_%d : %d\n", vi, ci, active[ci][vi]);
        }
    
        for(uint32_t vi = 1; vi < num_prop; vi++)
        {
          // same as above but pivots are not treated differently from normal variables
          uint32_t else_branch = makeAND(cond3, cumulative[parent2][vi]);
          uint32_t resolv = makeOR(cumulative[parent1][vi], cumulative[parent2][vi]);
          uint32_t interm = makeITE(cond2, cumulative[parent1][vi], else_branch);
          debugf("%d = if %d then %d else %d\n", interm, cond2, cumulative[parent1][vi], else_branch);
          cumulative[ci][vi] = makeAND(cumulative[ci][vi], makeITE(cond1, resolv, interm));
          debugf("%d = if %d then %d else %d\n", cumulative[ci][vi], cond1, resolv, interm);
          debugf("cumulative for %d_%d : %d\n", vi, ci, cumulative[ci][vi]);
        }
      }
    }

    // go through all variable of this quantifier and look through the cumulative activities at the root node
    // indicators for var being true are the cumulative of prop. variables which are annotated with var = true
    // a corresponding definition applies to indicators of var being false
    // the output for the variable is then pos_ind & -neg_ind
    for(const_var_iterator vit = quant->begin(); vit != quant->end(); vit++)
    {
      Lit l_pos = make_lit(*vit, false);
      Lit l_neg = make_lit(*vit, true);

      auto lpiter = indicators.find(l_pos);
      auto lniter = indicators.find(l_neg);

      uint32_t pos_ind = aiger_false;
      uint32_t neg_ind = aiger_false;
      uint32_t solution = aiger_false;

      if(lniter == indicators.end())
        solution = aiger_true;
      else if(lpiter == indicators.end())
        solution = aiger_false;
      else
      {
        for(const Var v : lpiter->second)
          pos_ind = makeOR(pos_ind, cumulative[root][v]);

        for(const Var v : lniter->second)
          neg_ind = makeOR(neg_ind, cumulative[root][v]);

        neg_ind = aiger_not(neg_ind);

        if(pos_ind == aiger_true || pos_ind == aiger_false)
          solution = pos_ind;
        else if(neg_ind == aiger_true || neg_ind == aiger_false)
          solution = neg_ind;
        else
          solution = pos_ind; // (lpiter->second.size() < lniter->second.size()) ? pos_ind : neg_ind;
      }
      aiger_add_and(aig, aiger_var2lit(*vit), solution, solution);
      uint64_t node = make_node(solution, solution);
      node_cache[node] = aiger_var2lit(*vit);
      inv_node_cache[aiger_var2lit(*vit)] = node;
      aiger_add_output(aig, aiger_var2lit(*vit), nullptr);
    }
  }
  checkOscillation();
  aiger_prune(aig);
  return 0;
}

int FerpManager::writeAiger(FILE* file)
{
  return !aiger_write_to_file(aig, aiger_ascii_mode, file);
}

void FerpManager::collectPivots()
{
  pivots.clear();
  pivots.resize(trace_clauses.size(), 0);
  // assumes that everything is ok with the proof
  for(int i = 1; i < trace_clauses.size(); i++)
  {
    if(antecedents[i]->at(1) == 0) continue;
  
    const std::vector<Lit>* parent1 = trace_clauses[cnf_id_to_trace_id[antecedents[i]->at(0)]];
    const std::vector<Lit>* parent2 = trace_clauses[cnf_id_to_trace_id[antecedents[i]->at(1)]];
    
    auto li1 = parent1->begin();
    auto li2 = parent2->begin();
    
    while(li1 != parent1->end() && li2 != parent2->end())
    {
      const Var v1 = var(*li1);
      const Var v2 = var(*li2);
      if(v1 < v2)
        li1++;
      else if(v1 > v2)
        li2++;
      else if(*li1 == *li2)
        li1++, li2++;
      else
        pivots[i] = v1, li1 = parent1->end();
    }
    assert(pivots[i] != 0);
  }
}

void FerpManager::collectIndicators()
{
  indicators.clear();
  for(const auto& p2a : prop_to_annotation)
  {
    const std::vector<Lit>& anno = *(p2a.second);
    for(const Lit l : anno)
    {
      auto ind_iter = indicators.find(l);
      if(ind_iter == indicators.end())
        ind_iter = indicators.insert(std::pair<Lit, std::vector<Var>>(l, std::vector<Var>())).first;
      ind_iter->second.push_back(p2a.first);
    }
  }
}

FerpManager::Redundancy FerpManager::findRedundant(uint32_t a, uint32_t b)
{
  if(aiger_sign(b) == 1) return RED_NONE;
  
  auto inv_iter = inv_node_cache.find(b);
  if(inv_iter == inv_node_cache.end()) return RED_NONE;

  uint32_t left = node_first(inv_iter->second);
  uint32_t right = node_second(inv_iter->second);
  if(a == left || a == right)
    return RED_OTHER;
    
  if(a == aiger_not(left) || a == aiger_not(right))
    return RED_FALSE;
  
  // if we still don't have a match, try DFS into children
  Redundancy rec = findRedundant(a, left);
  if(rec != RED_NONE) return rec;
  
  return findRedundant(a, right);
}

void FerpManager::checkOscillation()
{
  std::set<uint32_t> permanent;
  std::set<uint32_t> temporary;

  for(int oi = 0; oi < aig->num_outputs; oi++)
  {
    if(triv_out.find(aig->outputs[oi].lit) != triv_out.end()) continue;

    temporary.clear();
    auto n_iter = inv_node_cache.find(aig->outputs[oi].lit);
    assert(n_iter != inv_node_cache.end());
    debugf("checking output %d\n", n_iter->first);
    checkOscilationHelper(permanent, temporary, n_iter->first);
  }
}

void FerpManager::checkOscilationHelper(std::set<uint32_t>& permanent, std::set<uint32_t>& temporary, uint32_t current)
{
  assert(aiger_sign(current) == 0);
  if(permanent.find(current) != permanent.end()) return;
  
  bool missing = temporary.insert(current).second;
  assert(missing);
  auto n_iter = inv_node_cache.find(current);
  uint64_t node; uint32_t left, right;
  
  if(n_iter == inv_node_cache.end())
    goto cleanup;
  
  node = n_iter->second;
  left = aiger_strip(node_first(node));
  right = aiger_strip(node_second(node));
  checkOscilationHelper(permanent, temporary, left);
  checkOscilationHelper(permanent, temporary, right);
cleanup:
  permanent.insert(current);
  temporary.erase(current);
}

#endif // FERP_CERT
