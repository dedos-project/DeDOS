#ifndef NDIDVALUE_H_
#define NDIDVALUE_H_

#include <bitset>
#include <gmpxx.h>
#include <stdint.h>
#include "NDvalue.h"
#include "NDint32-value.h"
#include "NDbool-value.h"

const uint32_t ND_ID_LEN = 160;
const string ND_MAX_ID_HEX_STRING = "ffffffffffffffffffffffffffffffffffffffff";
const mpz_class MAX_ID (ND_MAX_ID_HEX_STRING, 16);

class ND_IdValue: public ND_Value
{
public:

  ND_IdValue (string value = "0", uint32_t base = 2);

  virtual ~ND_IdValue ();

  virtual string ToString () const;

  virtual string ToFractionString () const;

  bitset<ND_ID_LEN> GetIdValue () const;

  virtual ND_Value* Clone () const;

  virtual bool Equals (const ND_Value* v) const;

  virtual ND_Value* Eval (Ndlog_Operator op, ND_Value* rhs = NULL);

  virtual mpz_class GetMpz () const;

protected:

  bitset<ND_ID_LEN> m_value;
};

#endif