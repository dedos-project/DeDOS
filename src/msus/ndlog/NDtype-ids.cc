#include "value/NDtype-ids.h" 

string
GetTypeIDName (NDValueTypeId type)
{
  switch (type)
  {
    case NIL: return "nil";
    case ANYTYPE: return "anytype";
    case INT32: return "int32";
    case REAL: return "real";
    case STR: return "str";
    case IPV4: return "ipv4";
    case BOOL: return "bool";
    case LIST: return "list";
    case ID:  return "id";
    //case SV: return "sv";
    case BYTE_ARRAY: return "byte-array";
    default: return "unknown";
  }
}