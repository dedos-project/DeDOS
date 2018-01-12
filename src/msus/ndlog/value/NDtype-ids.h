#ifndef NDTYPEID_H
#define NDTYPEID_H

#include <string>

using namespace std;

enum NDValueTypeId
{
  NIL,
  ANYTYPE,
  INT32,
  REAL,
  STR,
  IPV4,
  BOOL,
  LIST,
  ID,
  // SV,
  BYTE_ARRAY,
};

string
GetTypeIDName (NDValueTypeId type);

#endif