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
 * @file stats.c
 *
 * Functions for the sending and receiving of statsitics between ctrl and runtime
 */
#include "stats.h"
#include "logging.h"

#include <stdlib.h>

size_t serialize_stat_samples(struct stat_sample *samples, int n_samples, void **buffer) {
    *buffer = samples;
    return sizeof(*samples) * n_samples;
}

int deserialize_stat_samples(void *buffer, size_t buff_len,
                             struct stat_sample **samples) {
    if (buff_len % sizeof(*samples) != 0) {
        log_error("Cannot deserialize stat samples! Buffer of incorrect size");
        return -1;
    }
    int n_samples = buff_len / sizeof(**samples);
    *samples = buffer;
    return n_samples;
}
