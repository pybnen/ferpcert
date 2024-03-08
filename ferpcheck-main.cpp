#include <zlib.h>
#include <memory>

#include "FerpReader.h"
#include "QbfReader.h"
#include <sys/resource.h>

// taken from qrpcheck
static inline double read_cpu_time()
{
  struct rusage u;
  if (getrusage (RUSAGE_SELF, &u))
    return 0;
  return u.ru_utime.tv_sec + 1e-6 * u.ru_utime.tv_usec +
         u.ru_stime.tv_sec + 1e-6 * u.ru_stime.tv_usec;
}


int main(int argc, const char* argv[])
{
  if(argc != 3)
  {
    printf("usage: %s <QBF> <FERP>\n", argv[0]);
    return -1;
  }
  
  const char* qbf_name = argv[1];
  const char* ferp_name = argv[2];
  
  double start_time = read_cpu_time();

  gzFile qbf_file = gzopen(qbf_name, "rb");
  
  if (qbf_file == Z_NULL)
  {
    printf("Could not open file: %s", qbf_name);
    return -2;
  }

  double start_qbf_read = read_cpu_time();
  Formula qbf;
  {
    std::unique_ptr<QbfReader> qbf_reader(new QbfReader(qbf_file));
    int res = qbf_reader->readQBF(qbf);
    gzclose(qbf_file);
    if (res != 0)
    {
      printf("Something went wrong while reading QBF, code %d\n", res);
      return res;
    }
  }
  double qbf_read_time = read_cpu_time() - start_qbf_read;
  printf("FerpCheck read QBF: %.6f s\n", qbf_read_time);

  gzFile ferp_file = gzopen(ferp_name, "rb");
  
  if (ferp_file == Z_NULL)
  {
    printf("Could not open file: %s", ferp_name);
    return -3;
  }

  double start_ferp_read = read_cpu_time();
  std::unique_ptr<FerpManager> fmngr(new FerpManager());
  {
    std::unique_ptr<FerpReader> ferp_reader(new FerpReader(ferp_file));

    int res = ferp_reader->readFERP(*fmngr);
    gzclose(ferp_file);
    if (res != 0)
    {
      printf("Something went wrong while reading FERP, code %d\n", res);
      return res;
    }
  }
  double ferp_read_time = read_cpu_time() - start_ferp_read;
  printf("FerpCheck read FERP: %.6f s\n", ferp_read_time);

  int res = fmngr->check(qbf);
  if(res != 0)
  {
    printf("Something went wrong while checking FERP, code %d\n", res);
    return res;
  }
  
  /* print stats */
  double cpu_time = read_cpu_time() - start_time;
  printf("FerpCheck check nor: %.6f s\n", fmngr->check_nor_time);
  printf("FerpCheck check elimination: %.6f s\n", fmngr->check_elimination_time);
  printf("FerpCheck sat solver called %d times\n", fmngr->sat_calls);
  printf("FerpCheck sat solver: %.6f s\n", fmngr->check_sat_time);
  printf("FerpCheck find assigment: %.6f s\n", fmngr->find_assignment_time);
  printf("FerpCheck eliminate clauses: %.6f s\n", fmngr->eliminate_clauses_time);
  printf("FerpCheck check resolution: %.6f s\n", fmngr->check_resolution_time);
  printf("FerpCheck was running for %.6f s\n", cpu_time);
  return 0;
}