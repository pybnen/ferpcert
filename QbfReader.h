//
// Created by vedad on 24/08/18.
//

#ifndef FERPCHECK_QBFREADER_H
#define FERPCHECK_QBFREADER_H

#include "Reader.h"

class QbfReader : protected Reader
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
  QbfReader (gzFile& file) : Reader(file), type_(QuantType::NONE) {};
};


#endif //FERPCHECK_QBFREADER_H
