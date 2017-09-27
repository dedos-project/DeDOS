#ifndef NDBYTEARRAYVALUE_H_
#define NDBYTEARRAYVALUE_H_

#include "NDvalue.h"
#include <stdint.h>
#include <string.h>

using namespace std;

class ND_ByteArrayValue: public ND_Value
{
public:

  ND_ByteArrayValue (uint8_t* buf = NULL, uint32_t len = 0);

  virtual ~ND_ByteArrayValue ();

  virtual string ToString () const;

  virtual uint8_t* GetByteArrayPtr () const;

  virtual uint32_t GetByteArrayLen () const;

  virtual ND_Value* Clone () const;

  virtual bool Equals (const ND_Value* v) const;

  virtual ND_Value* Eval (Ndlog_Operator op, ND_Value* rhs = NULL); 

protected:

  uint8_t* m_array;

  uint32_t m_len;
};

#endif