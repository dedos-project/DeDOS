#ifndef NDNILVALUE_H
#define NDNILVALUE_H

#include "NDvalue.h"

class ND_NilValue: public ND_Value
{
public: 

  ND_NilValue();

  virtual ~ND_NilValue();

  virtual string ToString () const;

  virtual ND_Value* Clone () const;

  virtual bool Equals (const ND_Value* v) const;

  virtual ND_Value* Eval (Ndlog_Operator op, ND_Value* rhs = NULL);

};

#endif