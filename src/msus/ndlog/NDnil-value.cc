#include "value/NDnil-value.h"

ND_NilValue::ND_NilValue ()
  : ND_Value (NIL)
{
}

ND_NilValue::~ND_NilValue ()
{
}

string
ND_NilValue::ToString () const
{
  return "nil";
}

ND_Value*
ND_NilValue::Clone () const
{
  return new ND_NilValue;
}

bool
ND_NilValue::Equals (const ND_Value* v) const
{
  return GetType () == v->GetType ();
}

ND_Value* 
ND_NilValue::Eval (Ndlog_Operator op, ND_Value* rhs) 
{
  return NULL;
}