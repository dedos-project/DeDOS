#include "value/NDbool-value.h"
#include <sstream>

ND_BoolValue::ND_BoolValue (bool value)
  : ND_Value (BOOL), m_value (value)
{
}

ND_BoolValue::~ND_BoolValue ()
{
}

string
ND_BoolValue::ToString () const
{
  stringstream ss;
  ss << (m_value ? "true" : "false");
  return ss.str ();
}

bool
ND_BoolValue::GetBoolValue () const
{
  return m_value;
}

ND_Value*
ND_BoolValue::Clone () const
{
  return new ND_BoolValue (m_value);
}

bool
ND_BoolValue::Equals (const ND_Value* v) const
{
  if(GetType () != v->GetType ())
    {
      return false;
    }

  const ND_BoolValue* other = dynamic_cast<const ND_BoolValue*> (v);

  return GetBoolValue () == other->GetBoolValue ();
}

ND_Value* 
ND_BoolValue::Eval (Ndlog_Operator op, ND_Value* rhs) 
{
  ND_Value* retval = NULL;

  switch (op) 
  {
    case ND_AND: 
      {
        if (!m_value)
        {
          return new ND_BoolValue (false);
        }

        if (dynamic_cast<ND_BoolValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value && (dynamic_cast<ND_BoolValue*> (rhs))->GetBoolValue());
        }
        else 
        {
          log_error("Error: ND_BoolValue: ND_AND: invalid operator.");
        }
        break;
      }
    case ND_OR: 
      {
        if (m_value)
        {
          return new ND_BoolValue (true);
        }

        if (dynamic_cast<ND_BoolValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value || (dynamic_cast<ND_BoolValue*> (rhs))->GetBoolValue());
        }
        else 
        {
          log_error("Error: ND_BoolValue: ND_OR: invalid operator.");
        }
        break;
      }
    case ND_NOT: 
      {
        retval = new ND_BoolValue (!m_value);
      }
    case ND_EQ: 
      {
        if (dynamic_cast<ND_BoolValue*> (rhs)) 
        {
          retval = new ND_BoolValue (Equals (rhs));
        }
        else 
        {
          log_error("Error: ND_BoolValue: ND_EQ: invalid operator.");
        }
        break;
      }
    case ND_NEQ: 
      {
        if (dynamic_cast<ND_BoolValue*> (rhs)) 
        {
          retval = new ND_BoolValue (!Equals (rhs));
        }
        else 
        {
          log_error("Error: ND_BoolValue: ND_NEQ: invalid operator.");
        }
        break;
      }
    default: 
      {
        log_error("Error: ND_BoolValue: Unknown operator.");
      }
  }
  return retval;
}