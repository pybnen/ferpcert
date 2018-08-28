//
// Created by vedad on 24/08/18.
//

#include "FerpReader.h"

int FerpReader::readFERP(FerpManager& mngr)
{
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
  
  while (true)
  {
    if (*stream == EOF) break;
    int res = readClause(mngr);
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
