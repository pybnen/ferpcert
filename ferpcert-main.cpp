#include <zlib.h>

#include "FerpReader.h"
#include "QbfReader.h"

int main(int argc, const char* argv[])
{
  if(argc != 4)
  {
    printf("usage: %s <QBF> <FERP> <AIGER>\n", argv[0]);
    return -1;
  }
  
  const char* qbf_name = argv[1];
  const char* ferp_name = argv[2];
  const char* aig_name = argv[3];
  
  gzFile qbf_file = gzopen(qbf_name, "rb");
  
  if (qbf_file == Z_NULL)
  {
    printf("Could not open file: %s", qbf_name);
    return -2;
  }
  
  gzFile ferp_file = gzopen(ferp_name, "rb");
  
  if (ferp_file == Z_NULL)
  {
    printf("Could not open file: %s", ferp_name);
    return -3;
  }
  
  FILE* aig_file = fopen(aig_name, "wb");
  
  if (aig_file == nullptr)
  {
    printf("Could not open file: %s", aig_name);
    return -4;
  }
  
  
  Formula qbf;
  QbfReader qbf_reader(qbf_file);
  
  int res = qbf_reader.readQBF(qbf, false);
  gzclose(qbf_file);
  if (res != 0)
  {
    printf("Something went wrong while reading QBF, code %d\n", res);
    return res;
  }
  
  FerpManager fmngr;
  FerpReader ferp_reader(ferp_file);
  
  res = ferp_reader.readFERP(fmngr);
  gzclose(ferp_file);
  if (res != 0)
  {
    printf("Something went wrong while reading FERP, code %d\n", res);
    return res;
  }
  
  fmngr.extract(qbf);
  res = fmngr.writeAiger(aig_file);
  fclose(aig_file);
  
  if(res != 0)
  {
    printf("Something went wrong while writing AIGER, code %d\n", res);
    return res;
  }
  
  return 0;
}