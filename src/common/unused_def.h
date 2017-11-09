/** @file unused_def.h Macro for declaring functions or variables as unused
 * to avoid compiler warnings*/
#ifndef DEFINES_H__
#define DEFINES_H__

#ifdef __GNUC__
/** Placed before a variable or function definition, will avoid the compiler warning
 * about an unused variable */
#define UNUSED __attribute__ ((unused))
#else
/** Placed before a variable or function definition, will avoid the compiler warning
 * about an unused variable */
#define UNUSED
#endif

#endif
