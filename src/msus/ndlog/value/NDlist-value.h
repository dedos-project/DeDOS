#ifndef NDLISTVALUE_H
#define NDLISTVALUE_H

#include <list>
#include <stdint.h>
#include "NDvalue.h"

using namespace std;

class ND_ListValue: public ND_Value
{
public: 
  
  ND_ListValue (list<ND_Value*> value = list<ND_Value*> ());

  virtual ~ND_ListValue();

  uint32_t Size() const;

  virtual string ToString() const;

  list<ND_Value*> GetListValue() const;

  virtual ND_Value* Clone () const;

  virtual bool Equals (const ND_Value* v) const;

  virtual ND_Value* Eval (Ndlog_Operator op, ND_Value* rhs = NULL);

  list<ND_Value*>::const_iterator Begin() const;

  list<ND_Value*>::const_iterator End() const;

  bool Contains (ND_Value* value) const;

protected: 
  
  list<ND_Value*> m_value;
};

#endif