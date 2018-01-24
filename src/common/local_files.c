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
#include "dedos.h"
/**
 * @file local_files.c
 *
 * Accessing local files within the repo
 */

#include <stdio.h>

/**
 * Set to the directory of the executabe
 */
static char *local_dir;

int set_local_directory(char *dir) {
    local_dir = dir;
    return 0;
}

int get_local_file(char *out, char *file) {
    sprintf(out, "%s/%s", local_dir, file);
    return 0;
}
