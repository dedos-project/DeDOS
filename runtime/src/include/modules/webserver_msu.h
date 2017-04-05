#ifndef WEBSERVER_MSU_H_
#define WEBSERVER_MSU_H_
/**
 * @file webserver_msu.h
 * Implementation for Webserver MSU, including HTML generation
 */

#include "generic_msu.h"

#define VIDEO_MEMORY 1000 * 1000 * 1000 * 2
#define AUDIO_MEMORY 1000 * 1000 * 1000 * 1
#define IMAGE_MEMORY 1000 * 1000 * 100
#define TEXT_MEMORY 1000 * 1000 * 10

/** All instances of a Webserver MSU share this type information. */
const struct msu_type WEBSERVER_MSU_TYPE;

#endif
