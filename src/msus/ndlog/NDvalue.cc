#include "value/NDvalue.h"

ND_Value::ND_Value (NDValueTypeId type)
  : m_type (type)
{
}

ND_Value::~ND_Value ()
{
}

NDValueTypeId
ND_Value::GetType () const
{
  return m_type;
}

string
ND_Value::GetTypeName () const
{
  return GetTypeIDName (m_type);
}

ostream& 
operator << (ostream& os, const ND_Value* value)
{
  os << value->ToString();
  return os;
}