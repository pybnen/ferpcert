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