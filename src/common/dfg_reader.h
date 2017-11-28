/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania, Georgetown University

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
END OF LICENSE STUB
*/
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
