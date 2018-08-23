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

class FerpManager
{
private:
  std::map<Var,Var> prop_to_original;
  std::map<Var, std::vector<Lit>*> prop_to_annotation;
  std::vector<std::vector<Lit>*> annotations;
  
  std::vector<std::vector<Lit>*> trace_clauses;      ///< Clauses as they appear in the trace
  std::vector<uint32_t> trace_id_to_cnf_id;          ///< Clause ids as they appear in the trace
  std::map<uint32_t, uint32_t> cnf_id_to_trace_id;   ///< Lookup table in other direction
  std::vector<std::array<uint32_t, 2>*> antecedents; ///< Stores antecedents of each trace clause
  
  std::vector<Lit> orig_ex;
  uint32_t root;
public:
  inline FerpManager();
  ~FerpManager();
  
  int addVariables(const std::vector<Var>& prop, const std::vector<Var>& orig, const std::vector<Lit>& anno);
  int addClause(uint32_t id, std::vector<Lit>* clause, std::array<uint32_t, 2>* ante);
  int check(const Formula& qbf);
  int checkExpansion(const Formula& qbf, uint32_t index);
  int checkResolution(uint32_t index);
  int checkRedundant();
};

//////////// INLINE IMPLEMENTATIONS ////////////

FerpManager::FerpManager() : root(0) {};

#endif //TOFERP_VARMENAGER_H
