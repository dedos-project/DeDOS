#ifndef NDSTRVALUE_H_
#define NDSTRVALUE_H_

#include <string>
#include "NDvalue.h"
#include "NDbool-value.h"

class ND_StrValue: public ND_Value
{
public: 

  ND_StrValue(string value = "");

  virtual ~ND_StrValue();

  virtual string ToString() const;

  string GetStrValue() const;

  virtual ND_Value* Clone () const;

  virtual bool Equals (const ND_Value* v) const;

  virtual ND_Value* Eval (Ndlog_Operator op, ND_Value* rhs = NULL);

protected: 
  
  string m_value;
};

#endif