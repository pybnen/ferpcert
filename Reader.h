//
// Created by vedad on 07/10/17.
//

#ifndef FERPCHECK_READER_H
#define FERPCHECK_READER_H

#include "parseutils.h"
#include "common.h"
#include "FerpManager.h"
#include "Formula.h"

#include <vector>

/// Reader class for parsing files
class Reader
{
protected:
  StreamBuffer stream; ///< Input stream
  
  /// Parses an unsigned integer, which is a Var
  int parseUnsigned(unsigned& ret);
  
  /// Parses a signed integer, which is a Lit
  int parseSigned(int& ret);
public:
  /// Reader Constructor
  Reader (gzFile& file) : stream(file) {}
};

class QBFReader : protected Reader
{
private:
  QuantType type_;             ///< Quantifier type of the current quantifier
  std::vector<Var> variables_; ///< Variable quantified under the current quantifier
  std::vector<Lit> clause_;    ///< Current clause
  
  /// Reads and discards comments from #stream_
  void readComments();
  
  /// Reads QDIMACS header
  int readHeader(unsigned& nvars, unsigned& nclauses);
  
  /// Reads the whole prefix and adds it to \a f
  int readPrefix(Formula& f);
  
  /// Reads a single quantifier, called by readPrefix(Formula&)
  int readQuant(Formula& f);
  
  /// Reads the whole matrix and adds it to \a f
  int readMatrix(Formula& f);
  
  /// Reads a single clause, called by readMatrix(Formula&)
  int readClause();
public:
  /// Reads the whle QBF and stores it in \a f
  int readQBF(Formula& f);
  
  /// Reader Constructor
  QBFReader (gzFile& file) : Reader(file), type_(QuantType::NONE) {};
};

class FERPReader : protected Reader
{
private:
  int readExpansions(FerpManager& mngr);
  int readResolutions(FerpManager& mngr);
  int readClause(FerpManager& mngr);
public:
  int readFERP(FerpManager& mngr);
  FERPReader(gzFile& file) : Reader(file) {};
};

#endif // FERPCHECK_READER_H
