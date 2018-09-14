//
// Created by vedad on 21/08/18.
//

#include <stdint.h>
#include <stdio.h>
#include <algorithm>

#include "FerpManager.h"

FerpManager::~FerpManager()
{
  for(std::vector<Lit>* a : annotations)
    delete a;
  for(std::vector<Lit>* c : trace_clauses)
    delete c;
  for(std::array<uint32_t, 2>* a : antecedents)
    delete a;

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
  for(const Lit l : *clause)
    if(prop_to_original.find(var(l)) == prop_to_original.end()) return 2;
  
  if(root != 0 && clause->empty()) return 3;
  
  if(clause->empty())
    root = (uint32_t)trace_clauses.size();
  
  std::sort(clause->begin(), clause->end(), lit_order);
  trace_clauses.push_back(clause);
  antecedents.push_back(ante);
  
  return 0;
}

#ifdef FERP_CHECK
int FerpManager::check(const Formula& qbf)
{
  for(uint32_t i = 1; i < trace_clauses.size(); i++)
  {
    if(antecedents[i]->at(1) == 0)
    {
      // clause comes from axiom rule
      int res = checkExpansion(qbf, i);
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

int FerpManager::checkExpansion(const Formula& qbf, uint32_t index)
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

int FerpManager::checkResolution(uint32_t index)
{
  const std::vector<Lit>* prop_clause = trace_clauses[index];
  const std::vector<Lit>* parent1 = trace_clauses[cnf_id_to_trace_id[antecedents[index]->at(0)]];
  const std::vector<Lit>* parent2 = trace_clauses[cnf_id_to_trace_id[antecedents[index]->at(1)]];
  
  // find the pivot and check resolvent at the same time
  std::vector<Var> pivot_candidates;
  auto li1 = parent1->begin();
  auto li2 = parent2->begin();
  auto res = prop_clause->begin();
  
  while(li1 != parent1->end() && li2 != parent2->end())
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
  while(li1 != parent1->end())
  {
    if(*li1 != *res) return 9;
    li1++; res++;
  }
  
  while(li2 != parent2->end())
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
  std::vector<bool> mark(trace_clauses.size(), false);
  
  // apply active, cumulative, and top rule for leaf clauses
  // * top rule:  clause is satisfied if the part of the caluse containing literals from a previous layer is satisfied
  // * active rule : variable is active if it is not assigned yet and contained in the clause and clause is not satisfied
  // * cumulative rule : same as active for leaf clauses
  ////   example qbf  e 1 2 3 a 4 5 e 6 7 . (-1 | 3 | 5 | 7) & ...
  ////   proof leaf: (-1 | 3 | 7^{4 -5})      top: (-1 | 3)
  ////                                     active:  1 : false, 2 : false, 3: false, 6^{4 -5}: false, 7^{4 -5}: (1 & -3)
  ////                                 cumulative:  1 : false, 2 : false, 3: false, 6^{4 -5}: false, 7^{4 -5}: (1 & -3)
  for(uint32_t ci = 1; ci < trace_clauses.size(); ci++)
  {
    if(pivots[ci] != 0) continue;
    
    mark[ci] = true;
    const std::vector<Lit>* clause = trace_clauses[ci];
    
    // generate cube of negated literals in topmost existential quantifier
    // \todo it might be possible to optimise this further:
    // by looking for existing sub-clause encodings
    uint32_t out = aiger_true;
    for(uint32_t li = 0; li < clause->size(); li++)
    {
      const Lit l = clause->at(li);
      if(qbf.getVarDepth(prop_to_original[var(l)]) != 0) continue;
      
      uint32_t rhs = make_aiger_lit(prop_to_original[var(l)], !sign(l));
      
      if(out == aiger_true)
        out = rhs;
      else
        out = makeAND(out, rhs);
    }
    
    // the clause is satisfied by assignment if the part of the clause
    // containing literals from topmost existential quantifier is satisfied
    top[ci] = aiger_not(out);
    
    for(uint32_t li = 0; li < clause->size(); li++)
    {
      const Lit l = clause->at(li);
      if(qbf.getVarDepth(prop_to_original[var(l)]) == 0) continue;
      
      active[ci][var(l)] = out;
      cumulative[ci][var(l)] = out;
    }
  }
  
  // go through the rest of the proof in some topological order: antecedents always come before the resolvent
  // * top rule: a clause is true if the pivot is not active in both parents and
  //                                 parent 1 is true or contains the pivot and
  //                                 parent 2 is true or contains the pivot
  // * active rule: if the variable is the pivot then it cannot be active
  //                if both parents both have active pivot then each variable is active if it is active in either parent
  //                if parent 1 is not true and does not have active pivot: inherit activity from parent1
  //                if parent 2 is not true and does not have active pivot: inherit activity from parent1
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
      
      // clause is set to true if none of the conditions apply
      top[ci] = makeAND(makeAND(aiger_not(cond1), aiger_not(cond2)), aiger_not(cond3));
      
      for(uint32_t vi = 1; vi < num_prop; vi++)
      {
        if(vi == pivot) continue; // pivot cannot be active in the resolvent
        
        // optimisation: condition 3 is true and variable is active in parent 2
        uint32_t else_branch = makeAND(cond3, active[parent2][vi]);
        // active in either parent
        uint32_t resolv = makeOR(active[parent1][vi], active[parent2][vi]);
        // big if then else for deciding from where the activity comes
        active[ci][vi] = makeITE(cond1, resolv, makeITE(cond2, active[parent1][vi], else_branch));
      }
  
      for(uint32_t vi = 1; vi < num_prop; vi++)
      {
        // same as above but pivots are not treated differently from normal variables
        uint32_t else_branch = makeAND(cond3, cumulative[parent2][vi]);
        uint32_t resolv = makeOR(cumulative[parent1][vi], cumulative[parent2][vi]);
        cumulative[ci][vi] = makeITE(cond1, resolv, makeITE(cond2, cumulative[parent1][vi], else_branch));
      }
    }
  }
  
  
  // go through all variable of this quantifier and look through the cumulative activities at the root node
  // indicators for var being true are the cumulatives of prop. variables which are annotated with var = true
  // a corresponding definition applies to indicators of var being false
  // the output for the variable is then pos_ind & -neg_ind
  for(uint32_t qi = 0; qi < qbf.numQuants(); qi++)
  {
    const Quant* quant = qbf.getQuant(qi);
    if(quant->type == QuantType::FORALL)
    {
      for(const_var_iterator vit = quant->begin(); vit != quant->end(); vit++)
      {
        Lit l_pos = make_lit(*vit, false);
        Lit l_neg = make_lit(*vit, true);
        
        auto lpiter = indicators.find(l_pos);
        auto lniter = indicators.find(l_neg);
  
        uint32_t pos_ind = aiger_false;
        uint32_t neg_ind = aiger_false;
        
        if(lniter == indicators.end())
          pos_ind = aiger_true, neg_ind = aiger_true;
        else if(lpiter == indicators.end())
          pos_ind = aiger_false, neg_ind = aiger_false;
        else
        {
          for(const Var v : lpiter->second)
            pos_ind = makeOR(pos_ind, cumulative[root][v]);
  
          for(const Var v : lniter->second)
            neg_ind = makeOR(neg_ind, cumulative[root][v]);
          
          neg_ind = aiger_not(neg_ind);
        }
        aiger_add_and(aig, aiger_var2lit(*vit), pos_ind, neg_ind);
        uint64_t node = make_node(pos_ind, neg_ind);
        node_cache[node] = aiger_var2lit(*vit);
        inv_node_cache[aiger_var2lit(*vit)] = node;
        aiger_add_output(aig, aiger_var2lit(*vit), nullptr);
      }
      break;
    }
  }
  
  checkOscilation();
  
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

FerpManager::Redundancy FerpManager::find_redundant(uint32_t a, uint32_t b)
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
  Redundancy rec = find_redundant(a, left);
  if(rec != RED_NONE) return rec;
  
  return find_redundant(a, right);
}

void FerpManager::checkOscilation()
{
  std::set<uint32_t> permanent;
  std::set<uint32_t> temporary;
  for(int oi = 0; oi < aig->num_outputs; oi++)
  {
    temporary.clear();
    auto n_iter = inv_node_cache.find(aig->outputs[oi].lit);
    assert(n_iter != inv_node_cache.end());
    printf("checking output %d\n", n_iter->first);
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
