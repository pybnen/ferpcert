#include <zlib.h>

#include "FerpReader.h"
#include "QbfReader.h"

int main(int argc, const char* argv[])
{
  if(argc != 3)
  {
    printf("usage: %s <QBF> <FERP>\n", argv[0]);
    return -1;
  }
  
  const char* qbf_name = argv[1];
  const char* ferp_name = argv[2];
  
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
  
  Formula qbf;
  QbfReader qbf_reader(qbf_file);
  
  int res = qbf_reader.readQBF(qbf);
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
  
  res = fmngr.check(qbf);
  if(res != 0)
  {
    printf("Something went wrong while checking FERP, code %d\n", res);
    return res;
  }
  
  return 0;
}