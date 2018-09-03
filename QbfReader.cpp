//
// Created by vedad on 24/08/18.
//

#include <iostream>
#include "QbfReader.h"

int QbfReader::readQBF(Formula& f)
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
    printf("c Warning: Variable count mismatch %ld %d\n", f.numVars(), num_vars);
  if (f.numClauses() != num_clauses)
    printf("c Warning: Clause count mismatch %ld %d\n", f.numClauses(), num_clauses);
  return 0;
}

void QbfReader::readComments()
{
  while (*stream == 'c') skipLine(stream);
}

int QbfReader::readHeader(unsigned& nvars, unsigned& nclauses)
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

int QbfReader::readPrefix(Formula& f)
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

int QbfReader::readQuant(Formula& f)
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

int QbfReader::readMatrix(Formula& f)
{
  
  while (true)
  {
    if (*stream == EOF) break;
    if (readClause()) return 6;
    f.addClause(clause_);
  }
  
  return 0;
}

int QbfReader::readClause()
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
