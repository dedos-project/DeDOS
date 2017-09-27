#ifndef NDINT32VALUE_H
#define NDINT32VALUE_H

/* 
** INT32 deal with uint32_t
*/

#include "NDvalue.h"
#include "NDreal-value.h"
#include "NDbool-value.h"
#include <stdint.h>

using namespace std;

class ND_Int32Value: public ND_Value
{
public: 
  
  ND_Int32Value(int32_t value = 0);

  virtual ~ND_Int32Value();

  virtual string ToString() const;

  uint32_t GetInt32Value() const;

  virtual ND_Value* Clone () const;

  virtual bool Equals (const ND_Value* v) const;

  virtual ND_Value* Eval (Ndlog_Operator op, ND_Value* rhs = NULL);

protected: 
  
  uint32_t m_value;
};

#endif