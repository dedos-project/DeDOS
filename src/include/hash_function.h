#ifndef HASH_FUNCTION_H_
#define HASH_FUNCTION_H_
#include <openssl/sha.h>
#include "logging.h"

unsigned int compute_hash(int id);
void print_hash(unsigned char* hash);

#endif
