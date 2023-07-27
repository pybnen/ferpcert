//
// Created by vedad on 24/08/18.
//

#ifndef FERPCHECK_FERPREADER_H
#define FERPCHECK_FERPREADER_H

#include "Reader.h"

class FerpReader : protected Reader
{
private:
  int readExpansions(FerpManager& mngr);
  int readResolutions(FerpManager& mngr);
  int readClause(FerpManager& mngr);
  int readClauseSAT(FerpManager& mngr, bool expansion_part);
  void readSATLine(FerpManager& mngr);
public:
  int readFERP(FerpManager& mngr);
  FerpReader(gzFile& file) : Reader(file) {};
};


#endif //FERPCHECK_FERPREADER_H
