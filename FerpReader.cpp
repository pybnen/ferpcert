//
// Created by vedad on 24/08/18.
//

#include "FerpReader.h"

int FerpReader::readFERP(FerpManager& mngr)
{
  // TODO do not skip s line
  // TOOD if s 1 check for sat, else if s 0 check for unsat
  consumeSATLine();

  if(readExpansions(mngr)) return 1;
  if(readResolutions(mngr)) return 2;
  return 0;
}

int FerpReader::readExpansions(FerpManager& mngr)
{
  std::vector<Var> propositional;
  std::vector<Var> original;
  std::vector<Lit> annotation;
  
  while(*stream == 'x')
  {
    ++stream;
    skipWhitespace(stream);
    Var v; Lit l;
    propositional.clear();
    while (true)
    {
      if (parseUnsigned(v)) return 1;
      if (v == 0) break;
      propositional.push_back(v);
    }
    original.clear();
    while (true)
    {
      if (parseUnsigned(v)) return 2;
      if (v == 0) break;
      original.push_back(v);
    }
    if(propositional.size() != original.size()) return 3;
    annotation.clear();
    while (true)
    {
      if (parseSigned(l)) return 4;
      if (l == 0) break;
      annotation.push_back(l);
    }
    if(mngr.addVariables(propositional, original, annotation)) return 5;
  }
  return 0;
}

int FerpReader::readResolutions(FerpManager& mngr)
{
  // skip id 0
  mngr.addClause(0, new std::vector<Lit>(), new std::array<uint32_t, 2>());

  bool expansion_part = true;
  
  while (true)
  {
    if (*stream == EOF) break;
    if (*stream == 'r') {
      skipLine(stream);
      expansion_part = false;
      continue;
    }

    int res = readClause(mngr, expansion_part);
    if (res)
    {
      printf("Reading clause failed with error code %d\n", res);
      return 6;
    }
  }
  
  return 0;
}

#define clean(val) do {ret = val; goto cleanup;} while(0)

int FerpReader::readClause(FerpManager& mngr, bool expansion_part)
{
  int ret = 0;
  bool is_nor_clause = true;
  std::vector<Lit>* clause = new std::vector<Lit>();
  std::vector<uint32_t>* original_clause_ids = new std::vector<uint32_t>();
  std::array<uint32_t, 2>* ante = new std::array<uint32_t, 2>();
  Var helper_variable = 0;
  std::vector<Lit>* literal_array = new std::vector<Lit>();
  
  Lit l = 0;
  uint32_t index = 0;
  if (parseUnsigned(index)) clean(1);
  while (true)
  {
    if (parseSigned(l)) clean(2);
    if (l == 0) break;
    clause->push_back(l);

    if (expansion_part) {
      if (!sign(l) && mngr.isHelper(var(l))) {
        helper_variable = var(l);
        is_nor_clause = false;
      } else {
        literal_array->push_back(l);
      }
    }  
  }
  
  if (!expansion_part)
  {
    if (parseUnsigned(ante->at(0))) clean(3);
    if (ante->at(0) == 0) clean(4);
    if (parseUnsigned(ante->at(1))) clean(5);
    // must contain 2 references
    if (ante->at(0) == 0) clean(4);
    if (parseSigned(l)) clean(6);
    if (l != 0) clean(7);
    mngr.res_clause_ids.push_back(mngr.trace_clauses.size()); 
  }
  else
  {
    uint32_t original_clause_id = 0;
    while (true)
    {
      if (parseUnsigned(original_clause_id)) clean(3);
      if (original_clause_id == 0) break;
      original_clause_ids->push_back(original_clause_id);
    }
    
    if (is_nor_clause)
    {
      assert(original_clause_ids->size() == clause->size());
      mngr.original_clause_mapping.push_back(original_clause_ids);
      
      std::vector<Lit>* copy_clause = new std::vector<Lit>();
      for (auto lit : *clause) {
        copy_clause->push_back(lit);
      }
      mngr.nor_clauses.push_back(copy_clause);
    } else {
      if (mngr.helper_variable_mapping.count(helper_variable) > 0) {
        auto helper_arr = mngr.helper_variable_mapping[helper_variable];
        helper_arr->insert(helper_arr->end(), literal_array->begin(), literal_array->end());
        delete literal_array;
      } else {
        mngr.helper_variable_mapping[helper_variable] = literal_array;
      }      
    }
  }
  
  if (mngr.addClause(index, clause, ante)) clean(8);
  return 0;
cleanup:
  delete clause;
  delete ante;
  return ret;
}

void FerpReader::consumeSATLine()
{
  while (*stream == 's') skipLine(stream);
}