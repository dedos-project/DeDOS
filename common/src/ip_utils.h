#ifndef IP_UTILS_H_
#define IP_UTILS_H_

#include <inttypes.h>

int how_many_digits(int num);

uint32_t long_from(void *_p);

int is_digit(char c);

int string_to_ipv4(const char *ipstr, uint32_t *ip);

int ipv4_to_string(char *ipbuf, const uint32_t ip);

#endif
