#include "value/NDreal-value.h"
#include <sstream>

ND_RealValue::ND_RealValue (double value) 
  : ND_Value (REAL), m_value (value)
{
}

ND_RealValue::~ND_RealValue ()
{
}

string
ND_RealValue::ToString () const
{
  stringstream ss;
  ss << m_value;
  return ss.str ();
}

ND_Value*
ND_RealValue::Clone () const
{
  return new ND_RealValue (m_value);
}

double
ND_RealValue::GetRealValue () const
{
  return m_value;
}

bool
ND_RealValue::Equals (const ND_Value* v) const
{
  if(GetType () != v->GetType ())
    {
      return false;
    }

  const ND_RealValue* other = dynamic_cast<const ND_RealValue*> (v);

  return GetRealValue () == other->GetRealValue ();
}

ND_Value*
ND_RealValue::Eval (Ndlog_Operator op, ND_Value* rhs) 
{
  ND_Value* retval = NULL;

  switch (op) 
  {
    case ND_PLUS: 
      {
        if (dynamic_cast<ND_RealValue*> (rhs)) 
        {
          retval = new ND_RealValue (m_value + (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_RealValue (m_value + (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else 
        {
          log_error("Error: ND_RealValue: ND_PLUS: invalid operator");
        }
        break;
      }
    case ND_MINUS: 
      {
        if (dynamic_cast<ND_RealValue*> (rhs)) 
        {
          retval = new ND_RealValue (m_value - (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_RealValue (m_value - (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else 
        {
          log_error("Error: ND_RealValue: ND_MINUS: invalid operator");
        }
        break;
      }
    case ND_TIMES: 
      {
        if (dynamic_cast<ND_RealValue*> (rhs)) 
        {
          retval = new ND_RealValue (m_value * (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_RealValue (m_value * (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else 
        {
          log_error("Error: ND_RealValue: ND_TIMES: invalid operator");
        }
        break;
      }
    case ND_DIVIDE: 
      {
        if (dynamic_cast<ND_RealValue*> (rhs)) 
        {
          retval = new ND_RealValue (m_value / (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_RealValue (m_value / (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else 
        {
          log_error("Error: ND_RealValue: ND_DIVIDE: invalid operator");
        }
        break;
      }
    case ND_EQ: 
      {
        if (dynamic_cast<ND_RealValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value == (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value == (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else 
        {
          log_error("Error: ND_RealValue: ND_EQ: invalid operator");
        }
        break;
      }
    case ND_NEQ: 
      {
        if (dynamic_cast<ND_RealValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value != (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value != (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else 
        {
          log_error("Error: ND_RealValue: ND_NEQ: invalid operator");
        }
        break;
      }
    case ND_GT: 
      {
        if (dynamic_cast<ND_RealValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value > (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value > (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else 
        {
          log_error("Error: ND_RealValue: ND_GT: invalid operator");
        }
        break;
      }
    case ND_LT: 
      {
        if (dynamic_cast<ND_RealValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value < (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value < (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else 
        {
          log_error("Error: ND_RealValue: ND_LT: invalid operator");
        }
        break;
      }
    case ND_GTE: 
      {
        if (dynamic_cast<ND_RealValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value >= (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value >= (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else 
        {
          log_error("Error: ND_RealValue: ND_GTE: invalid operator");
        }
        break;
      }
    case ND_LTE: 
      {
        if (dynamic_cast<ND_RealValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value <= (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value <= (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else 
        {
          log_error("Error: ND_RealValue: ND_LTE: invalid operator");
        }
        break;
      }
    default: 
      {
        log_error("Error: ND_RealValue: unknown operator");
      }
  }
  return retval;
}