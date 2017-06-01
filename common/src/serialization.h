#pragma once
#include <stdlib.h>
#include <string.h>

/** Gets the amount of space taken up by n repetitions of a struct
 * @param st - struct to measure
 * @param n  - n repetitions
 * @return size in bytes of n repetitions of st
 */
#define SERIAL_SIZE(st, n)\
    sizeof(*st) * n

/** Serializes  an object into an already-allocated buffer
 * @param buff - The buffer into which to serialize
 * @param st   - Pointer to the item to be serialized (must be typed, not void)
 * @param n    - number of repetitions of st
 * @return     - Pointer to the end of the buffer
 */
#define SERIALIZE(buff, st)\
   (memcpy(buff, st, SERIAL_SIZE(st, 1)) + SERIAL_SIZE(st, 1))

/** Serializes n repetitions of an object into an already-allocated buffer
 * @param buff - The buffer into which to serialize
 * @param st   - Pointer to the item to be serialized (must be typed, not void)
 * @param n    - number of repetitions of st
 * @return     - Pointer to the end of the buffer
 */
#define SERIALIZE_N(buff, st, n)\
    (memcpy(buff, st, SERIAL_SIZE(st, n)) + SERIAL_SIZE(st, n))

/** Reallocates a buffer, then serializes n repetitions of an object into it
 * @param buff   The buffer into which to serialize
 * @param offset The offset into the buffer at which to place the object
 * @param st     Pointer to the item to be serialized (must be typed, not void)
 * @return       The amount of space used in the serialization on success, -1 on error
 */
#define ALLOC_SERIALIZE(buff, offset, st)\
    ((buff = realloc(buff, (offset) + SERIAL_SIZE(st, 1))) != NULL ? SERIALIZE(buff+(offset), st) : NULL)

/** Reallocates a buffer, then serializes n repetitions of an object into it
 * @param buff   The buffer into which to serialize
 * @param offset The offset into the buffer at which to place the object
 * @param st     Pointer to the item to be serialized (must be typed, not void)
 * @return       The amount of space used in the serialization on success, -1 on error
 */
#define ALLOC_SERIALIZE_N(buff, offset, st, n)\
    ((buff = realloc(buff, (offset) + SERIAL_SIZE(st, n))) != NULL ? SERIALIZE_N(buff+(offset), st, n) : NULL)


/** Copies a serialized representation of the object into an allocated structure
 * @param buff  - The buffer containing the serialized structure
 * @param st    - Pointer to the structure to be allocated
 * @param n     - Number of repetitions of st
 * @return      - Pointer to the next section in the buffer if successful, otherwise NULL
 */
#define COPY_DESERIALIZE(buff, st)\
    ((memcpy(st, buff, SERIAL_SIZE(st, 1))) != NULL ? buff + SERIAL_SIZE(st, 1) : NULL)

/** Copies a serialized representation of n objects into an allocated structure
 * @param buff  - The buffer containing the serialized structure
 * @param st    - Pointer to the structure to be allocated
 * @param n     - Number of repetitions of st
 * @return      - Pointer to the next section in the buffer if successful, otherwise NULL
 */
#define COPY_DESERIALIZE_N(buff, st, n)\
    ((memcpy(st, buff, SERIAL_SIZE(st, n))) != NULL ? buff + SERIAL_SIZE(st, n) : NULL)


/** Allocates a structre, then copies a serialized representation of the object into it
 * @param buff  - The buffer containing the serialized structure
 * @param st    - Pointer to the structure to be allocated
 * @param n     - Number of repetitions of st
 * @return      - The number of bytes used in the deserialization on success, -1 on error
 */
#define ALLOC_DESERIALIZE(buff, st)\
    ((st = realloc(st, SERIAL_SIZE(st, 1))) != NULL ? \
        COPY_DESERIALIZE(buff, st) : NULL)

/** Allocates a structre, then copies a serialized representation of the object into it
 * @param buff  - The buffer containing the serialized structure
 * @param st    - Pointer to the structure to be allocated
 * @param n     - Number of repetitions of st
 * @return      - The number of bytes used in the deserialization on success, -1 on error
 */
#define ALLOC_DESERIALIZE_N(buff, st, n)\
    ((st = realloc(st, SERIAL_SIZE(st, n))) != NULL ? \
        COPY_DESERIALIZE_N(buff, st, n) : NULL)

/** Points a structure to the appropriate location in a serialized object
 * @param buff  - The serialized structure
 * @param st    - Pointer to the structure which will be assigned to the approriate
 *                  location in buff
 * @param n     - The number of repetitions of st
 * @return      - The next location in the buffer
 */
#define DESERIALIZE(buff, st)\
    ((st = (void*) buff) + SERIAL_SIZE(st, 1))

/** Points a structure to the appropriate location in a serialized object
 * @param buff  - The serialized structure
 * @param st    - Pointer to the structure which will be assigned to the approriate
 *                  location in buff
 * @param n     - The number of repetitions of st
 * @return      - The number of deserialized bytes
 */
#define DESERIALIZE_N(buff, st, n)\
    ((st = (void*) buff) != NULL ? SERIAL_SIZE(st, n))
