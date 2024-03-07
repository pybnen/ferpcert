//
// Created by vedad on 24/08/18.
//

#include "FerpReader.h"

int FerpReader::readFERP(FerpManager& mngr)
{
  readSATLine(mngr);
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

    int res;
    if (mngr.is_sat) {
      res = readClauseSAT(mngr, expansion_part);
    } else {
      res = readClause(mngr);
    }

    if (res)
    {
      printf("Reading clause failed with error code %d\n", res);
      return 6;
    }
  }
  
  return 0;
}

#define clean(val) do {ret = val; goto cleanup;} while(0)

int FerpReader::readClause(FerpManager& mngr)
{
  int ret = 0;
  std::vector<Lit>* clause = new std::vector<Lit>();
  std::array<uint32_t, 2>* ante = new std::array<uint32_t, 2>();
  
  Lit l = 0;
  uint32_t index = 0;
  if (parseUnsigned(index)) clean(1);
  while (true)
  {
    if (parseSigned(l)) clean(2);
    if (l == 0) break;
    clause->push_back(l);
  }
  
  if (parseUnsigned(ante->at(0))) clean(3);
  if (ante->at(0) == 0) clean(4);
  if (parseUnsigned(ante->at(1))) clean(5);
  if (ante->at(1) != 0)
  {
    if (parseSigned(l)) clean(6);
    if (l != 0) clean(7);
  }
  
  if (mngr.addClause(index, clause, ante)) clean(8);
  return 0;
cleanup:
  delete clause;
  delete ante;
  return ret;
}

#define cleanSAT(val) do {ret = val; goto cleanupsat;} while(0)

int FerpReader::readClauseSAT(FerpManager& mngr, bool expansion_part)
{
  int ret = 0;
  bool is_nor_clause = true;
  std::vector<Lit>* clause = new std::vector<Lit>();
  std::array<uint32_t, 2>* ante = new std::array<uint32_t, 2>();
  Var helper_variable = 0;
  std::vector<Lit>* literal_array = new std::vector<Lit>();
  
  Lit l = 0;
  uint32_t index = 0;
  if (parseUnsigned(index)) cleanSAT(1);
  while (true)
  {
    if (parseSigned(l)) cleanSAT(2);
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
    delete literal_array;

    if (parseUnsigned(ante->at(0))) cleanSAT(3);
    if (ante->at(0) == 0) cleanSAT(4);
    if (parseUnsigned(ante->at(1))) cleanSAT(5);
    // must contain 2 references
    if (ante->at(0) == 0) cleanSAT(4);
    if (parseSigned(l)) cleanSAT(6);
    if (l != 0) cleanSAT(7);
    mngr.res_clause_ids.push_back(mngr.trace_clauses.size()); 
  }
  else
  {

    std::vector<std::vector<uint32_t>*> *oo = new std::vector<std::vector<uint32_t>*>();
    while (true)
    {
      std::vector<uint32_t> *originals = new std::vector<uint32_t>();
      uint32_t original_clause_id = 0;

      if (parseUnsigned(original_clause_id)) cleanSAT(3);
      if (original_clause_id == 0) {
        assert(originals->size() == 0);
        delete originals;
        break;
      }
      
      originals->push_back(original_clause_id);
      while (true)
      {
        if (parseUnsigned(original_clause_id)) cleanSAT(3);
        if (original_clause_id == 0) break;
        originals->push_back(original_clause_id);
      }
      oo->push_back(originals);
    }
    // uint32_t original_clause_id = 0;
    // while (true)
    // {
    //   if (parseUnsigned(original_clause_id)) cleanSAT(3);
    //   if (original_clause_id == 0) break;
    //   original_clause_ids->push_back(original_clause_id);
    // }
    
    if (is_nor_clause)
    {
      delete literal_array;
      assert(oo->size() == clause->size());
      mngr.original_clause_mapping.push_back(oo);
      
      std::vector<Lit>* copy_clause = new std::vector<Lit>();
      for (auto lit : *clause) {
        copy_clause->push_back(lit);
      }
      mngr.nor_clauses.push_back(copy_clause);
    } else {
      assert(oo->size() == 0);
      delete oo;

      if (mngr.helper_variable_mapping.count(helper_variable) > 0) {
        auto helper_arr = mngr.helper_variable_mapping[helper_variable];
        helper_arr->insert(helper_arr->end(), literal_array->begin(), literal_array->end());
        delete literal_array;
      } else {
        mngr.helper_variable_mapping[helper_variable] = literal_array;
      }      
    }
  }
  
  if (mngr.addClause(index, clause, ante)) cleanSAT(8);
  return 0;
cleanupsat:
  delete clause;
  delete ante;
  return ret;
}

void FerpReader::readSATLine(FerpManager& mngr)
{
  while (*stream == 's') {
    ++stream;
    skipWhitespace(stream);
    unsigned is_sat;
    parseUnsigned(is_sat);
    mngr.is_sat = (is_sat == 1);
  }
}