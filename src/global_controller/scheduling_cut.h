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
#ifndef _SCHEDULING_CUT_H
#define _SCHEDULING_CUT_H

struct cut {
    uint16_t num_cores;
    uint64_t dram;
    uint64_t io_network_bw;
    uint64_t egress_bw;
    uint64_t ingress_bw;
    int num_msu;
    int *msu_ids[MAX_MSU];
};

#endif
