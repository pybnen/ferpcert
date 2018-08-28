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


