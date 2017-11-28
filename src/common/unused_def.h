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
