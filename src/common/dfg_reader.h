/**
 * @file dfg_reader.h
 *
 * Declares function for converting JSON to dedos_dfg
 */
#ifndef DFG_READER_H_
#define DFG_READER_H_

#include "dfg.h"

/**
 * Converts a json file to a dfg structure
 * @param filename The json file to parse
 * @return A copy of the newly-allocated DFG, or NULL if error
 */
struct dedos_dfg *parse_dfg_json_file(const char *filename);

#endif
