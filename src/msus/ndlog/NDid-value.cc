#include "value/NDid-value.h"
#include <sstream>

ND_IdValue::ND_IdValue (string value, uint32_t base)
  : ND_Value (ID)
{
  mpz_class temp (value, base);
  m_value = bitset<ND_ID_LEN> (temp.get_str (2));
}

ND_IdValue::~ND_IdValue ()
{
}

string
ND_IdValue::ToString () const
{
  mpz_class hex (m_value.to_string (), 2);
  return hex.get_str (16);
}

string
ND_IdValue::ToFractionString () const
{
  mpf_class hex (m_value.to_string (), 2);
  mpf_class max (ND_MAX_ID_HEX_STRING);
  mp_exp_t x;
  mpf_class result = hex / max;

  return result.get_str (x, 10, 3);
}

bitset<ND_ID_LEN>
ND_IdValue::GetIdValue () const
{
  return m_value;
}

ND_Value*
ND_IdValue::Clone () const
{
  // Passed by value
  return new ND_IdValue (m_value.to_string ());
}


bool
ND_IdValue::Equals (const ND_Value* v) const
{
  if(GetType () != v->GetType ())
    {
      return false;
    }

  const ND_IdValue* other = dynamic_cast<const ND_IdValue*> (v);

  return GetIdValue () == other->GetIdValue ();
}

ND_Value* 
ND_IdValue::Eval (Ndlog_Operator op, ND_Value* rhs)
{
  ND_Value* retval = NULL;

  switch (op) 
  {
    case ND_PLUS: 
      {
        if (dynamic_cast<ND_IdValue*> (rhs)) 
        {
          mpz_class result = GetMpz() + (dynamic_cast<ND_IdValue*> (rhs))->GetMpz();
          retval = new ND_IdValue (result.get_str (2));
        }
        else if (dynamic_cast<ND_Int32Value*> (rhs))
        {
          mpz_class arg ((dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
          mpz_class result = GetMpz() + arg;
          retval = new ND_IdValue (result.get_str (2));
        }
        else 
        {
          log_error("Error: ND_IdValue: ND_PLUS: invalid operator");
        }
        break;
      } 
    case ND_MINUS: 
      {
        mpz_class result; 
        if (dynamic_cast<ND_IdValue*> (rhs)) 
        {
          result = GetMpz() - (dynamic_cast<ND_IdValue*> (rhs))->GetMpz();
        }
        else if (dynamic_cast<ND_Int32Value*> (rhs))
        {
          mpz_class arg ((dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value());
          result = GetMpz() - arg;
        }
        else 
        {
          log_error("Error: ND_IdValue: ND_MINUS: invalid operator");
        }
        // Handle -ve values
        if (result < mpz_class ("0"))
        {
          result = MAX_ID + mpz_class ("1") + result;
        }
        retval = new ND_IdValue (result.get_str (2));
        break;
      }
    case ND_LSHIFT: 
      {
        if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_IdValue ();
          (dynamic_cast<ND_IdValue*> (retval))->m_value = m_value << ((dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value()) ;
        }
        else 
        {
          log_error("Error: ND_IdValue: ND_LSHIFT: invalid operator");
        }
        break;
      }
    case ND_RSHIFT: 
      {
        if (dynamic_cast<ND_Int32Value*> (rhs)) 
        {
          retval = new ND_IdValue ();
          (dynamic_cast<ND_IdValue*> (retval))->m_value = m_value >> ((dynamic_cast<ND_Int32Value*> (rhs))->GetInt32Value()) ;
        }
        else 
        {
          log_error("Error: ND_IdValue: ND_RSHIFT: invalid operator");
        }
        break;
      }
    case ND_EQ: 
      {
        if (dynamic_cast<ND_IdValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value == ((dynamic_cast<ND_IdValue*> (rhs))->GetIdValue()));
        }
        else 
        {
          log_error("Error: ND_IdValue: ND_EQ: invalid operator");
        }
        break;
      }
    case ND_NEQ: 
      {
        if (dynamic_cast<ND_IdValue*> (rhs)) 
        {
          retval = new ND_BoolValue (m_value != ((dynamic_cast<ND_IdValue*> (rhs))->GetIdValue()));
        }
        else 
        {
          log_error("Error: ND_IdValue: ND_NEQ: invalid operator");
        }
        break;
      }
    case ND_LT: 
      {
        if (dynamic_cast<ND_IdValue*> (rhs)) 
        {
          ND_IdValue* arg = dynamic_cast<ND_IdValue*> (rhs);
          retval = new ND_BoolValue (GetMpz() < arg->GetMpz());
        }
        else 
        {
          log_error("Error: ND_IdValue: ND_LT: invalid operator");
        }
        break;
      }
    case ND_GT: 
      {
        if (dynamic_cast<ND_IdValue*> (rhs)) 
        {
          ND_IdValue* arg = dynamic_cast<ND_IdValue*> (rhs);
          retval = new ND_BoolValue (GetMpz() > arg->GetMpz());
        }
        else 
        {
          log_error("Error: ND_IdValue: ND_GT: invalid operator");
        }
        break;
      }
    case ND_LTE: 
      {
        if (dynamic_cast<ND_IdValue*> (rhs)) 
        {
          ND_IdValue* arg = dynamic_cast<ND_IdValue*> (rhs);
          retval = new ND_BoolValue (GetMpz() <= arg->GetMpz());
        }
        else 
        {
          log_error("Error: ND_IdValue: ND_LTE: invalid operator");
        }
        break;
      }
    case ND_GTE: 
      {
        if (dynamic_cast<ND_IdValue*> (rhs)) 
        {
          ND_IdValue* arg = dynamic_cast<ND_IdValue*> (rhs);
          retval = new ND_BoolValue (GetMpz() >= arg->GetMpz());
        }
        else 
        {
          log_error("Error: ND_IdValue: ND_GTE: invalid operator");
        }
        break;
      }
    default: 
      {
        log_error("Error: ND_IdValue: unknown operator");
      }
  }
  return retval;
}

mpz_class
ND_IdValue::GetMpz () const
{
  return mpz_class (ToString (), 16);
}