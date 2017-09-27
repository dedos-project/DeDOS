#ifndef NDVALUE_H
#define NDVALUE_H

#include <string>
#include "NDtype-ids.h"
#include "logging.h"

typedef enum Ndlog_Operator
{
  ND_NO_OP = 0, // No operation
  ND_PLUS,
  ND_MINUS,
  ND_TIMES,
  ND_DIVIDE,
  ND_MODULUS,
  ND_LSHIFT,
  ND_RSHIFT,
  ND_NOT,
  ND_AND,
  ND_OR,
  ND_EQ,
  ND_NEQ,
  ND_GT,
  ND_LT,
  ND_GTE,
  ND_LTE,
  ND_IN_RANGE, // what is its use? TODO: implement operator 
  ND_BIT_XOR,
  ND_BIT_AND,
  ND_BIT_OR,
  ND_BIT_NOT,
} Ndlog_Operator;

class ND_Value 
{
public: 

  ND_Value (NDValueTypeId value);

  virtual ~ND_Value ();

  NDValueTypeId GetType () const;

  string GetTypeName() const;

  virtual string ToString() const = 0;

  virtual ND_Value* Clone () const = 0;

  virtual bool Equals (const ND_Value* v) const = 0;

  virtual ND_Value* Eval (Ndlog_Operator op, ND_Value* rhs = NULL) = 0;

protected: 

  NDValueTypeId m_type;
};

ostream& operator << (ostream& os, const ND_Value* value);

#endif
