#include "value/NDbyte-array-value.h"
#include <sstream>

ND_ByteArrayValue::ND_ByteArrayValue (uint8_t* buf, uint32_t len) :
  ND_Value (BYTE_ARRAY), m_array (buf), m_len (len)
{
}

ND_ByteArrayValue::~ND_ByteArrayValue ()
{
  if (m_array != NULL)
    {
      delete[] m_array;
    }
}

string
ND_ByteArrayValue::ToString () const
{
  stringstream ss;
  ss << "<byte-array[" << m_len << "]>";
  return ss.str ();
}

ND_Value*
ND_ByteArrayValue::Clone () const
{
  uint8_t* newArray = new uint8_t[m_len];
  memcpy (newArray, m_array, m_len);
  return new ND_ByteArrayValue (newArray, m_len);
}

uint8_t*
ND_ByteArrayValue::GetByteArrayPtr  () const
{
  return m_array;
}

uint32_t
ND_ByteArrayValue::GetByteArrayLen () const
{
  return m_len;
}

bool
ND_ByteArrayValue::Equals (const ND_Value* v) const
{
  if (GetType () != v->GetType ())
    {
      return false;
    }
  const ND_ByteArrayValue* other = dynamic_cast<const ND_ByteArrayValue*> (v);

  if ( (m_len == other->GetByteArrayLen ()) && (memcmp (m_array,
    other->GetByteArrayPtr (), m_len) == 0))
    {
      return true;
    }
  else
    {
      return false;
    }
}

ND_Value* 
ND_ByteArrayValue::Eval (Ndlog_Operator op, ND_Value* rhs)
{
  ND_Value* retval = NULL;

  switch (op) 
  {
    default: 
      {
        log_error("Error: ND_ByteArrayValue: ND_ByteArrayValue does not support any operator.");
      }
  }
  return retval;
}