#ifndef NDREALVALUE_H
#define NDREALVALUE_H

#include "NDvalue.h"
#include "NDint32-value.h"
#include "NDbool-value.h"

using namespace std;

class ND_RealValue: public ND_Value 
{
public: 
  
  ND_RealValue(double value = 0.0);

  virtual ~ND_RealValue();

  virtual string ToString() const;

  double GetRealValue() const;

  virtual ND_Value* Clone () const;

  virtual bool Equals (const ND_Value* v) const;

  virtual ND_Value* Eval (Ndlog_Operator op, ND_Value* rhs = NULL);

protected: 
  
  double m_value;
};

#endif