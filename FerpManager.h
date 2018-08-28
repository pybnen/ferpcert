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
  
  std::vector<std::vector<Lit>*> trace_clauses;      ///< Clauses as they appear in the trace
  std::vector<uint32_t> trace_id_to_cnf_id;          ///< Clause ids as they appear in the trace
  std::map<uint32_t, uint32_t> cnf_id_to_trace_id;   ///< Lookup table in other direction
  std::vector<std::array<uint32_t, 2>*> antecedents; ///< Stores antecedents of each trace clause
  std::vector<Var> pivots;                           ///< Stores pivot of each resolved trace clause
  
  std::vector<Lit> orig_ex;
  uint32_t root;
#ifdef FERP_CERT
  aiger* aig;
#endif
  
#ifdef FERP_CHECK
  int checkExpansion(const Formula& qbf, uint32_t index);
  int checkResolution(uint32_t index);
  int checkRedundant();
#endif
#ifdef FERP_CERT
  void collectPivots();
#endif
public:
  inline FerpManager();
  ~FerpManager();
  
  int addVariables(const std::vector<Var>& prop, const std::vector<Var>& orig, const std::vector<Lit>& anno);
  int addClause(uint32_t id, std::vector<Lit>* clause, std::array<uint32_t, 2>* ante);
#ifdef FERP_CHECK
  int check(const Formula& qbf);
#endif
#ifdef FERP_CERT
  int extract(const Formula& qbf);
#endif
};

//////////// INLINE IMPLEMENTATIONS ////////////

FerpManager::FerpManager() : root(0) {};

#endif //FERPCHECK_FERPMENAGER_H
