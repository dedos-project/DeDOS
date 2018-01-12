#include "value/NDstr-value.h"
#include <sstream>

ND_StrValue::ND_StrValue (string value)
  : ND_Value (STR), m_value (value)
{
}

ND_StrValue::~ND_StrValue ()
{
}

string 
ND_StrValue::ToString() const
{
  stringstream ss;
  ss << m_value;
  return ss.str ();
}

string 
ND_StrValue::GetStrValue() const
{
  return m_value;
}

ND_Value*
ND_StrValue::Clone () const
{
  return new ND_StrValue (m_value);
}

bool
ND_StrValue::Equals (const ND_Value* v) const
{
  if(GetType () != v->GetType ())
    {
      return false;
    }

  const ND_StrValue* other = dynamic_cast<const ND_StrValue*> (v);

  return GetStrValue () == other->GetStrValue ();
}

ND_Value*
ND_StrValue::Eval (Ndlog_Operator op, ND_Value* rhs)
{
  ND_Value* retval = NULL;

  switch (op) 
  {
    case ND_PLUS: 
      {
        retval = new ND_StrValue (m_value + rhs->ToString());
        break;
      }
    case ND_EQ: 
      {
        retval = new ND_BoolValue (Equals (rhs));
        break;
      }
    case ND_NEQ: 
      {
        retval = new ND_BoolValue (!Equals (rhs));
        break;
      }
    case ND_GT:
      {
        if (dynamic_cast<ND_StrValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value.compare ((dynamic_cast<ND_StrValue*> (rhs))->GetStrValue()) > 0);
        }
        else 
        {
          log_error("Error: ND_StrValue: ND_GT: invalid operator");
        }
        break;
      }
    case ND_LT: 
      {
        if (dynamic_cast<ND_StrValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value.compare ((dynamic_cast<ND_StrValue*> (rhs))->GetStrValue()) < 0);
        }
        else 
        {
          log_error("Error: ND_StrValue: ND_LT: invalid operator");
        }
        break;
      }
    case ND_GTE: 
      {
        if (dynamic_cast<ND_StrValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value.compare ((dynamic_cast<ND_StrValue*> (rhs))->GetStrValue()) >= 0);
        }
        else 
        {
          log_error("Error: ND_StrValue: ND_GTE: invalid operator");
        }
        break;
      }
    case ND_LTE: 
      {
        if (dynamic_cast<ND_StrValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value.compare ((dynamic_cast<ND_StrValue*> (rhs))->GetStrValue()) <= 0);
        }
        else 
        {
          log_error("Error: ND_StrValue: ND_GTE: invalid operator");
        }
        break;
      }
    default: 
      {
        log_error("Error: ND_StrValue: unknown operator");
      }
  }
  return retval;
}