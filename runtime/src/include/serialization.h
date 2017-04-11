#pragma once
#include <stdlib.h>
#include <string.h>

/** Gets the amount of space taken up by n repetitions of a struct
 * @param st - struct to measure
 * @param n  - n repetitions
 * @return size in bytes of n repetitions of st
 */
#define SERIAL_SIZE2(st, n)\
    sizeof(*st) * n

/** Gets the amount of space taken up by repetitions of two structs
 * @param st - struct to measure
 * @param n  - n repetitions
 * @param st2 - struct to measure
 * @param n2  - n repetitions
 * @return size in bytes of n repetitions of st 
 *         followed by n2 repetitions of st2
 */
#define SERIAL_SIZE4(st, n, st2, n2) \
    SERIAL_SIZE2(st, n) + SERIAL_SIZE2(st, n)

/** Gets the amount of space taken up by repetitions of two structs
 * @param st - struct to measure
 * @param n  - n repetitions
 * @param st2 - struct to measure
 * @param n2  - n repetitions
 * @param st3 - struct to measure
 * @param n3  - n repetitions
 * @return size in bytes of n repetitions of st 
 *         followed by n2 repetitions of st2 
 *         followed by n3 repetitions of st3
 */
#define SERIAL_SIZE6(st, n, st2, n2, st3, n3) \
    SERIAL_SIZE2(st, n) + SERIAL_SIZE4(st2, n2, st3, n3)

/** Serializes n repetitions of an object into an already-allocated buffer
 * @param buff - The buffer into which to serialize
 * @param st   - Pointer to the item to be serialized (must be typed, not void)
 * @param n    - number of repetitions of st
 * @return     - The amount of space used in the serialization on success, -1 on error
 */
#define SERIALIZE3(buff, st, n)\
    (buff != NULL) ? SERIAL_SIZE2(st, n) : -1; \
    memcpy(buff, st, SERIAL_SIZE2(st, n))

/** Serializes n repetitions of two objects into an already-allocated buffer
 * @param buff - The buffer into which to serialize
 * @param st   - Pointer to the item to be serialized (must be typed, not void)
 * @param n    - number of repetitions of st
 * @param st2  - Pointer to the second item to be serialized (must be typed, not void)
 * @param n2   - number of repetitions of st2
 * @return     - The amount of space used in the serialization on success, -1 on error
 */
#define SERIALIZE5(buff, st, n, st2, n2) \
    (buff != NULL) ? SERIAL_SIZE4(st, n, st2, n2) : -1; \
    memcpy(buff, st, SERIAL_SIZE2(st, n)); \
    memcpy(buff + SERIAL_SIZE2(st, n), st2, SERIAL_SIZE2(st2, n2))

/** Serializes n repetitions of three objects into an already-allocated buffer
 * @param buff - The buffer into which to serialize
 * @param st   - Pointer to the item to be serialized (must be typed, not void)
 * @param n    - number of repetitions of st
 * @param st2  - Pointer to the second item to be serialized (must be typed, not void)
 * @param n2   - number of repetitions of st2
 * @param st3  - Pointer to the third item to be serialized (must be typed, not void)
 * @param n3   - number of repetitions of st3
 * @return     - The amount of space used in the serialization on success, -1 on error
  */
#define SERIALIZE7(buff, st, n, st2, n2, st3, n3) \
    (buff != NULL) ? SERIAL_SIZE6(st, n, st2, n2, st3, n3) : -1; \
    memcpy(buff, st, SERIAL_SIZE2(st, n)); \
    memcpy(buff + SERIAL_SIZE2(st, n), st2, SERIAL_SIZE2(st2, n2)); \
    memcpy(buff + SERIAL_SIZE4(st, n, st2, n2), st3, SERIAL_SIZE2(st3, n3))


/** Allocates a buffer, then serializes n repetitions of an object into it
 * @param buff - The buffer into which to serialize
 * @param st   - Pointer to the item to be serialized (must be typed, not void)
 * @param n    - number of repetitions of st
 * @return     - The amount of space used in the serialization on success, -1 on error
 */
#define ALLOC_SERIALIZE3(buff, st, n)\
    (buff = malloc(SERIAL_SIZE2(st, n))) != NULL ? SERIAL_SIZE2(st, n) : -1; \
    SERIALIZE3(buff, st, n)

/** Allocates a buffer, then serializes n repetitions of two objects into it
 * @param buff - The buffer into which to serialize
 * @param st   - Pointer to the item to be serialized (must be typed, not void)
 * @param n    - number of repetitions of st
 * @param st2  - Pointer to the second item to be serialized (must be typed, not void)
 * @param n2   - number of repetitions of st2
 * @return     - The amount of space used in the serialization on success, -1 on error
 */
#define ALLOC_SERIALIZE5(buff, st, n, st2, n2)\
    (buff = malloc(SERIAL_SIZE4(st, n, st2, n2))) != NULL ?  \
            SERIAL_SIZE4(st, n, st2, n2) : -1; \
    SERIALIZE5(buff, st, n, st2, n2);

/** Allocates a buffer, then serializes n repetitions of three objects into it
 * @param buff - The buffer into which to serialize
 * @param st   - Pointer to the item to be serialized (must be typed, not void)
 * @param n    - number of repetitions of st
 * @param st2  - Pointer to the second item to be serialized (must be typed, not void)
 * @param n2   - number of repetitions of st2
 * @param st3  - Pointer to the third item to be serialized (must be typed, not void)
 * @param n3   - number of repetitions of st3
 * @return     - The amount of space used in the serialization on success, -1 on error
  */
#define ALLOC_SERIALIZE7(buff, st, n, st2, n2, st3, n3)\
    (buff = malloc(SERIAL_SIZE6(st, n, st2, n2, st3, n3))) != NULL ? \
            SERIAL_SIZE6(st, n, st2, n2, st3, n3) : -1; \
    SERIALIZE7(buff, st, n, st2, n2, st3, n3);

/** Allocates a structre, then copies a serialized representation of the object into it
 * @param buff  - The buffer containing the serialized structure
 * @param st    - Pointer to the structure to be allocated
 * @param n     - Number of repetitions of st
 * @return      - The number of bytes used in the deserialization on success, -1 on error
 */
#define COPY_DESERIALIZE3(buff, st, n)\
    (st = malloc(SERIAL_SIZE2(st, n))) != NULL && \
        (memcpy(st, buff, SERIAL_SIZE2(st, n))) != NULL ? \
        SERIAL_SIZE2(st, n) : -1;
 
/** Allocates structres, then copies a serialized representation of objects into them
 * @param buff  - The buffer containing the serialized structure
 * @param st    - Pointer to a structure to be allocated
 * @param n     - Number of repetitions of st
 * @param st2   - Pointer to a structure to be allocated
 * @param n2    - Number of repetitions of st2
 * @return      - The number of bytes used in the deserialization on success, -1 on error
 */       
#define COPY_DESERIALIZE5(buff, st, n, st2, n2)\
    (st = malloc(SERIAL_SIZE2(st, n))) != NULL && \
        (memcpy(st, buff, SERIAL_SIZE2(st, n))) != NULL && \
        (st2 = malloc(SERIAL_SIZE2(st2, n2))) != NULL && \
        (memcpy(st2, buff + SERIAL_SIZE2(st, n), SERIAL_SIZE2(st2, n2))) != NULL ? \
        SERIAL_SIZE4(st, n, st2, n2) : -1; \
    do { \
        if (st == NULL) \
            break; \
        else if (st2 == NULL) \
            free(st); \
    } while (0)

/** Allocates structres, then copies a serialized representation of objects into them
 * @param buff  - The buffer containing the serialized structure
 * @param st    - Pointer to a structure to be allocated
 * @param n     - Number of repetitions of st
 * @param st2   - Pointer to a structure to be allocated
 * @param n2    - Number of repetitions of st2
 * @param st3   - Pointer to a structure to be allocated
 * @param n3    - Number of repetitions of st3
 * @return      - The number of bytes used in the deserialization on success, -1 on error
 */     
#define COPY_DESERIALIZE7(buff, st, n, st2, n2, st3, n3)\
    (st = malloc(SERIAL_SIZE2(st, n))) != NULL && \
        (memcpy(st, buff, SERIAL_SIZE2(st, n))) != NULL && \
        (st2 = malloc(SERIAL_SIZE2(st2, n2))) != NULL && \
        (memcpy(st2, buff + SERIAL_SIZE2(st, n), SERIAL_SIZE2(st2, n2))) != NULL && \
        (st3 = malloc(SERIAL_SIZE2(st3, n3))) != NULL && \
        (memcpy(st3, buff + SERIAL_SIZE4(st, n, st2, n2), SERIAL_SIZE2(st3, n3))) != NULL ?\
        SERIAL_SIZE6(st, n, st2, n2, st3, n3) : -1; \
    do { \
        if (st == NULL) {\
            break; \
        } else if (st2 == NULL) {\
            free(st); \
        } else if (st3 == NULL) { \
            free(st2); \
            free(st); \
        } \
    } while (0)

/** Points a structure to the appropriate location in a serialized object
 * @param buff  - The serialized structure
 * @param st    - Pointer to the structure which will be assigned to the approriate
 *                  location in buff
 * @param n     - The number of repetitions of st
 * @return      - The number of deserialized bytes
 */  
#define DESERIALIZE3(buff, st, n)\
    ((st = (void*) buff) != NULL ? SERIAL_SIZE2(st, n) : -1)
        
/** Points structures to the appropriate locations in a serialized object
 * @param buff  - The serialized structure
 * @param st    - Pointer to the structure which will be assigned to the approriate
 *                  location in buff
 * @param n     - The number of repetitions of st
 * @param st2   - Pointer to the structure which will be assigned to the approriate
 *                  location in buff
 * @param n2    - The number of repetitions of st2
 * @return      - The number of deserialized bytes
 */  
#define DESERIALIZE5(buff, st, n, st2, n2)\
    ((st = (void*) buff) != NULL && \
     (st2 = (void*)( (char*)buff + SERIAL_SIZE2(st, n))) != NULL ? \
     SERIAL_SIZE4(st, n, st2, n2) : -1)
        
/** Points structures to the appropriate locations in a serialized object
 * @param buff  - The serialized structure
 * @param st    - Pointer to the structure which will be assigned to the approriate
 *                  location in buff
 * @param n     - The number of repetitions of st
 * @param st2   - Pointer to the structure which will be assigned to the approriate
 *                  location in buff
 * @param n2    - The number of repetitions of st2
 * @param st3   - Pointer to the structure which will be assigned to the approriate
 *                  location in buff
 * @param n3    - The number of repetitions of st2
 * @return      - The number of deserialized bytes
 */ 
#define DESERIALIZE7(buff, st, n, st2, n2, st3, n3)\
    ((st = (void*) buff) != NULL && \
     (st2 = (void*)( (char*)buff + SERIAL_SIZE2(st, n))) != NULL && \
     (st3 = (void*)( (char*)buff + SERIAL_SIZE4(st, n, st2, n2))) != NULL ? \
     SERIAL_SIZE6(st, n, st2, n2, st3, n3) : -1)
