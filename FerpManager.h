//
// Created by vedad on 21/08/18.
//

#ifndef FERPCHECK_FERPMENAGER_H
#define FERPCHECK_FERPMENAGER_H

#include <vector>
#include <map>
#include <set>
#include "common.h"
#include "Formula.h"

#ifdef FERP_CERT
#include <unordered_map>
extern "C" {
#include "aiger.h"
};
#endif // FERP_CERT

class FerpManager
{
private:
  std::map<Var,Var> prop_to_original;
  std::map<Var, std::vector<Lit>*> prop_to_annotation;
  std::vector<std::vector<Lit>*> annotations;
  
  std::vector<uint32_t> trace_id_to_cnf_id;          ///< Clause ids as they appear in the trace
  std::map<uint32_t, uint32_t> cnf_id_to_trace_id;   ///< Lookup table in other direction
  std::vector<std::array<uint32_t, 2>*> antecedents; ///< Stores antecedents of each trace clause
  
  std::vector<Lit> orig_ex;
  uint32_t root;
#ifdef FERP_CERT
  aiger* aig;                                            ///< AIG in which the model is stored
  uint32_t current_aig_var;                              ///< Current AIG variable, returned at next call to newVar()
  std::vector<Var> pivots;                               ///< Stores pivot of each resolved trace clause
  std::map<Lit, std::vector<Var>> indicators;            ///< Maps from annotation value to propositional variables
  std::unordered_map<uint64_t, uint32_t> node_cache;     ///< Cache Map for reusing AND gates
  std::unordered_map<uint32_t, uint64_t> inv_node_cache; ///< Inverse of node_cache
  std::set<uint64_t> triv_out;                           ///< Trivial outputs
  enum Redundancy {RED_NONE, RED_FALSE, RED_OTHER};      ///< Used for signaling in find_redundant
#endif
  
#ifdef FERP_CHECK
  int checkSAT(const Formula& qbf);
  int checkUNSAT(const Formula& qbf);
  int checkExpansionUNSAT(const Formula& qbf, uint32_t index);
  int checkResolution(uint32_t index);
  int checkRedundant();
  int checkExpansionSAT(const Formula& qbf, std::vector<Lit>* prop_clause, uint32_t origin_idx);
  int checkElimination(const Formula& qbf, uint32_t origin_idx, std::vector<Lit> assignment);
#endif
#ifdef FERP_CERT
  void collectPivots();
  void collectIndicators();
  inline uint32_t makeITE(uint32_t cond, uint32_t then_b, uint32_t else_b);
  inline uint32_t makeAND(uint32_t a, uint32_t b);
  inline uint32_t makeOR(uint32_t a, uint32_t b);
  inline uint32_t newVar();
  Redundancy findRedundant(uint32_t a, uint32_t b);
  void checkOscilationHelper(std::set<uint32_t>& permanent, std::set<uint32_t>& temporary, uint32_t current);
  void checkOscillation();
#endif
public:
  inline FerpManager();
  ~FerpManager();

  bool is_sat;
  /** maps helper variables to literals of clause that introduces the helper variable */
  std::map<Var, std::vector<Lit>*> helper_variable_mapping;
  std::vector<std::vector<Lit>*> nor_clauses;
  std::vector<uint32_t> res_clause_ids;
  std::vector<std::vector<uint32_t>*>  original_clause_mapping;
  bool isHelper(Var v);

  std::vector<std::vector<Lit>*> trace_clauses;      ///< Clauses as they appear in the trace
  
  int addVariables(const std::vector<Var>& prop, const std::vector<Var>& orig, const std::vector<Lit>& anno);
  int addClause(uint32_t id, std::vector<Lit>* clause, std::array<uint32_t, 2>* ante);
#ifdef FERP_CHECK
  int check(const Formula& qbf);
#endif
#ifdef FERP_CERT
  int extract(const Formula& qbf);
  int writeAiger(FILE* file);
#endif
};

//////////// INLINE IMPLEMENTATIONS ////////////

FerpManager::FerpManager() :
root(0),
is_sat(false)
#ifdef FERP_CERT
, aig(nullptr), current_aig_var(0)
#endif
{};

#ifdef FERP_CERT

#define make_node(a, b) \
(uint64_t)(((uint64_t)a << 32) | (uint64_t)b)

#define node_first(x) \
(uint32_t)((uint64_t)x >> 32)

#define node_second(x) \
(uint32_t)((uint64_t)x & ~(uint32_t)(0))

inline uint32_t FerpManager::makeITE(uint32_t cond, uint32_t then_b, uint32_t else_b)
{
  if(cond == aiger_true)
    return then_b;
  if(cond == aiger_false)
    return else_b;
  if(then_b == else_b)
    return then_b;


  uint32_t t = makeAND(cond, then_b);
  uint32_t e = makeAND(aiger_not(cond), else_b);
  return makeOR(t, e);
}

inline uint32_t FerpManager::makeAND(uint32_t a, uint32_t b)
{
  // simple rules of AND gates
  if(a == aiger_false || b == aiger_false)
    return aiger_false;
  if(a == aiger_true)
    return b;
  if(b == aiger_true)
    return a;
  if(a == b)
    return a;
  if(a == aiger_not(b))
    return aiger_false;
  
  // look for redundancy in form of  a & (a & x), a & (!a & x), ...
  
  Redundancy red = RED_NONE;
  red = findRedundant(a, b);
  if(red == RED_FALSE)
    return aiger_false;
  if(red == RED_OTHER)
    return b;
  
  red = findRedundant(b, a);
  if(red == RED_FALSE)
    return aiger_false;
  if(red == RED_OTHER)
    return a;
  
  // try looking for cached AND nodes
  uint64_t node = 0;
  node = make_node(b, a);
  auto node1_iter = node_cache.find(node);
  if(node1_iter != node_cache.end())
    return node1_iter->second;
  
  node = make_node(a, b);
  auto node2_iter = node_cache.find(node);
  if(node2_iter != node_cache.end())
    return node2_iter->second;
  
  uint32_t res = aiger_var2lit(newVar());
  aiger_add_and(aig, res, a, b);
  
  node_cache[node] = res;
  inv_node_cache[res] = node;
  
  return res;
}

inline uint32_t FerpManager::makeOR(uint32_t a, uint32_t b)
{
  return aiger_not(makeAND(aiger_not(a), aiger_not(b)));
}

inline uint32_t FerpManager::newVar()
{
  return current_aig_var++;
}
#endif

#endif //FERPCHECK_FERPMENAGER_H
