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



#endif // FERPCHECK_READER_H
