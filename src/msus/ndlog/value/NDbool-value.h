#ifndef NDBOOLVALUE_H_
#define NDBOOLVALUE_H_

#include "NDvalue.h"

using namespace std;

class ND_BoolValue: public ND_Value
{
public: 

  ND_BoolValue (bool value = false);

  virtual ~ND_BoolValue();

  virtual string ToString () const;

  bool GetBoolValue () const;

  virtual ND_Value* Clone () const;

  virtual bool Equals (const ND_Value* v) const; 

  virtual ND_Value* Eval (Ndlog_Operator op, ND_Value* rhs = NULL);

protected: 

  bool m_value;
};

#endif