//
// Created by vedad on 07/10/17.
//

#include "Reader.h"

#include <assert.h>
#include <algorithm>

int Reader::parseUnsigned(unsigned& ret)
{
  if (*stream < '0' || *stream > '9')
  {
    printf("Error while reading unsigned number\n");
    return 1;
  }
  
  unsigned result = 0;
  while (*stream >= '0' && *stream <= '9')
  {
    assert(result <= result * 10 + (*stream - '0'));
    result = result * 10 + (*stream - '0');
    ++stream;
  }
  ret = result;
  skipWhitespace(stream);
  return 0;
}

int Reader::parseSigned(int& ret)
{
  int sign = 1;
  
  if (*stream == '-')
  {
    sign = -1;
    ++stream;
  }
  
  if (*stream < '0' || *stream > '9')
  {
    printf("Error while reading signed number\n");
    return 2;
  }
  
  int result = 0;
  
  while (*stream >= '0' && *stream <= '9')
  {
    assert(result <= result * 10 + (*stream - '0'));
    result = result * 10 + (*stream - '0');
    ++stream;
  }
  ret = sign * result;
  skipWhitespace(stream);
  return 0;
}

int QBFReader::readQBF(Formula& f)
{
  // read the header
  readComments();
  unsigned num_vars = 0;
  unsigned num_clauses = 0;
  
  if (readHeader(num_vars, num_clauses)) return -1;
  if (readPrefix(f)) return -2;
  if (readMatrix(f)) return -3;
  f.finalise();
  if (f.numVars() != num_vars)
    printf("c Warning: Variable count mismatch\n");
  if (f.numClauses() != num_clauses)
    printf("c Warning: Clause count mismatch\n");
  return 0;
}

void QBFReader::readComments()
{
  while (*stream == 'c') skipLine(stream);
}

int QBFReader::readHeader(unsigned& nvars, unsigned& nclauses)
{
  if (!eagerMatch(stream, "p cnf "))
  {
    printf("Error while reading header\n");
    return 3;
  }
  
  skipWhitespace(stream);
  if (parseUnsigned(nvars)) return 3;
  if (parseUnsigned(nclauses)) return 3;
  return 0;
}

int QBFReader::readPrefix(Formula& f)
{
  while (true)
  {
    readComments();
    if (*stream != 'e' && *stream != 'a') break;
    if (readQuant(f)) return 4;
  }
  
  f.addQuantifier(type_, variables_);
  variables_.clear();
  
  return 0;
}

int QBFReader::readQuant(Formula& f)
{
  assert(*stream == 'e' || *stream == 'a');
  
  std::vector<bool> appearing_vars;
  
  QuantType type = (*stream == 'e') ? QuantType::EXISTS : QuantType::FORALL;
  
  ++stream;
  skipWhitespace(stream);
  
  if(type_ != type)
  {
    f.addQuantifier(type_, variables_);
    variables_.clear();
  }
  
  type_ = type;
  
  while (true)
  {
    if (*stream == EOF)
    {
      printf("Error when reading non-terminated quantifier\n");
      return 5;
    }
    Var v = 0;
    
    if (parseUnsigned(v)) return 5;
    
    if (v == 0) break;
    
    if (appearing_vars.size() <= v)
      appearing_vars.resize(v + 1, false);
    
    if (!appearing_vars[v])
      variables_.push_back(v);
    appearing_vars[v] = true;
  }
  
  return 0;
}

int QBFReader::readMatrix(Formula& f)
{
  
  while (true)
  {
    if (*stream == EOF) break;
    if (readClause()) return 6;
    f.addClause(clause_);
  }
  
  return 0;
}

int QBFReader::readClause()
{
  Lit l = 0;
  clause_.clear();
  while (true)
  {
    if (parseSigned(l)) return 7;
    if (l == 0) break;
    clause_.push_back(l);
  }
  return 0;
}

int FERPReader::readFERP(FerpManager& mngr)
{
  if(readExpansions(mngr)) return 1;
  if(readResolutions(mngr)) return 2;
  return 0;
}

int FERPReader::readExpansions(FerpManager& mngr)
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

int FERPReader::readResolutions(FerpManager& mngr)
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

int FERPReader::readClause(FerpManager& mngr)
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
