#include "value/NDint32-value.h"
#include <sstream>

ND_Int32Value::ND_Int32Value (int32_t value)
  : ND_Value (INT32), m_value (value)
{
}

ND_Int32Value::~ND_Int32Value ()
{
}

string
ND_Int32Value::ToString () const
{
  stringstream ss;
  ss << m_value;
  return ss.str ();
}

uint32_t ND_Int32Value::GetInt32Value() const
{
  return m_value;
}

ND_Value*
ND_Int32Value::Clone () const
{
  return new ND_Int32Value (m_value);
}

bool
ND_Int32Value::Equals (const ND_Value* v) const
{
  if(GetType () != v->GetType ())
    {
      return false;
    }

  const ND_Int32Value* other = dynamic_cast<const ND_Int32Value*> (v);

  return GetInt32Value () == other->GetInt32Value ();
}

ND_Value* 
ND_Int32Value::Eval (Ndlog_Operator op, ND_Value* rhs)
{
  ND_Value* retval = NULL;

  switch (op) 
  {
    case ND_PLUS: 
      {
        if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_Int32Value (m_value + (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else if (dynamic_cast<ND_RealValue*> (rhs))  
        {
          retval = new ND_RealValue (m_value + (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else 
        {
          log_error("Error: ND_Int32Value: ND_PLUS: invalid operator");
        }
        break;
      }
    case ND_MINUS: 
      {
        if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_Int32Value (m_value - (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else if (dynamic_cast<ND_RealValue*> (rhs))  
        {
          retval = new ND_RealValue (m_value - (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else 
        {
          log_error("Error: ND_Int32Value: ND_MINUS: invalid operator");
        }
        break;
      }
    case ND_TIMES: 
      {
        if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_Int32Value (m_value * (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else if (dynamic_cast<ND_RealValue*> (rhs))  
        {
          retval = new ND_RealValue (m_value * (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else 
        {
          log_error("Error: ND_Int32Value: ND_TIMES: invalid operator");
        }
        break;
      }
    case ND_DIVIDE: 
      {
        if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_Int32Value (m_value / (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else if (dynamic_cast<ND_RealValue*> (rhs))  
        {
          retval = new ND_RealValue (m_value / (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else 
        {
          log_error("Error: ND_Int32Value: ND_DIVIDE: invalid operator");
        }
        break;
      }
    case ND_MODULUS: 
      {
        if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_Int32Value (m_value % (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else 
        {
          log_error("Error: ND_Int32Value: ND_MODULUS: invalid operator");
        }
        break;
      }
    case ND_LSHIFT: 
      {
        if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_Int32Value (m_value << (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else 
        {
          log_error("Error: ND_Int32Value: ND_LSHIFT: invalid operator");
        }
        break;
      }
    case ND_RSHIFT: 
      {
        if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_Int32Value (m_value >> (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else 
        {
          log_error("Error: ND_Int32Value: ND_RSHIFT: invalid operator");
        }
        break;
      }
    case ND_BIT_AND: 
      {
        if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_Int32Value (m_value & (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else 
        {
          log_error("Error: ND_Int32Value: ND_BIT_AND: invalid operator");
        }
        break;
      }
    case ND_BIT_OR: 
      {
        if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_Int32Value (m_value | (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else 
        {
          log_error("Error: ND_Int32Value: ND_BIT_OR: invalid operator");
        }
        break;
      }
    case ND_BIT_XOR: 
      {
        if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_Int32Value (m_value ^ (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else 
        {
          log_error("Error: ND_Int32Value: ND_BIT_XOR: invalid operator");
        }
        break;
      }
    case ND_BIT_NOT: 
      {
        retval = new ND_Int32Value (~m_value);
        break;
      }
    case ND_EQ: 
      {
        if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value == (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else if (dynamic_cast<ND_RealValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value == (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else 
        {
          log_error("Error: ND_Int32Value: ND_EQ: invalid operator");
        }
        break;
      }
    case ND_NEQ: 
      {
        if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value != (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else if (dynamic_cast<ND_RealValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value != (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else 
        {
          log_error("Error: ND_Int32Value: ND_NEQ: invalid operator");
        }
        break;
      }
    case ND_GT: 
      {
        if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value > (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else if (dynamic_cast<ND_RealValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value > (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else 
        {
          log_error("Error: ND_Int32Value: ND_GT: invalid operator");
        }
        break;
      }
    case ND_LT: 
      {
        if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value < (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else if (dynamic_cast<ND_RealValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value < (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else 
        {
          log_error("Error: ND_Int32Value: ND_LT: invalid operator");
        }
        break;
      }
    case ND_GTE: 
      {
        if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value >= (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else if (dynamic_cast<ND_RealValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value >= (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else 
        {
          log_error("Error: ND_Int32Value: ND_GTE: invalid operator");
        }
        break;
      }
    case ND_LTE: 
      {
        if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value <= (dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
        }
        else if (dynamic_cast<ND_RealValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value <= (dynamic_cast<ND_RealValue*> (rhs))->GetRealValue());
        }
        else 
        {
          log_error("Error: ND_Int32Value: ND_LTE: invalid operator");
        }
        break;
      }
    default: 
      {
        log_error("Error: ND_Int32Value: unknown operator");
      }
  }
  return retval;
}
