#include "value/NDlist-value.h"
#include <sstream>

ND_ListValue::ND_ListValue (list<ND_Value*> value)
  :ND_Value (LIST), m_value (value)
{
}

ND_ListValue::~ND_ListValue ()
{
}

string
ND_ListValue::ToString() const
{
  stringstream ss;
  ss << "(";
  for (list<ND_Value*>::const_iterator it = Begin (); it != End (); ++it)
    {
      ss << (*it)->GetTypeName () << ":" << *it << ",";
    }

  string result = ss.str ();
  if (result.size () > 0)
    {
      result.replace (result.end () - 1, result.end (), ")");
    }
  return result;
}

ND_Value*
ND_ListValue::Clone () const
{
  list<ND_Value*> clone;
  for (list<ND_Value*>::const_iterator it = Begin (); it != End (); ++it)
    {
      clone.push_back ((*it)->Clone ());
    }
  return new ND_ListValue (clone);
}

bool
ND_ListValue::Equals (const ND_Value* v) const
{
  if(GetType () != v->GetType ())
    {
      return false;
    }

  const ND_ListValue* other = dynamic_cast<const ND_ListValue*> (v);
  if (Size () != other->Size ())
    {
      return false;
    }

  for (list<ND_Value*>::const_iterator it = Begin (), jt = other->Begin ();
    it != End () && jt != other->End (); ++it, jt++)
    {
      if ((*it)->Equals (*jt))
        {
          continue;
        }
      else
        {
          return false;
        }
    }
  return true;
}

ND_Value* 
ND_ListValue::Eval (Ndlog_Operator op, ND_Value* rhs) 
{
  ND_Value* retval = NULL;

  switch (op) 
  {
    case ND_PLUS: 
      {
        list<ND_Value*> newlist;
        for (list<ND_Value*>::const_iterator it = Begin(); it != End(); ++it) 
          {
            newlist.push_back((*it)->Clone());
          }
        newlist.push_back(rhs);
        retval = new ND_ListValue (newlist);
        break;
      }
    default: 
      {
        log_error("Error: ND_ListValue: unknown operator");
      }
  }
  return retval;
}

list<ND_Value*> 
ND_ListValue::GetListValue() const 
{
  return m_value;
}

uint32_t 
ND_ListValue::Size() const
{
  return m_value.size();
}

list<ND_Value*>::const_iterator 
ND_ListValue::Begin() const
{
  return m_value.begin();
}

list<ND_Value*>::const_iterator 
ND_ListValue::End() const
{
  return m_value.end();
}

bool 
ND_ListValue::Contains (ND_Value* value) const
{
  for (list<ND_Value*>::const_iterator it = Begin (); it != End (); ++it)
    {
      if ((*it)->Equals (value))
        {
          return true;
        }
    }
  return false;
}