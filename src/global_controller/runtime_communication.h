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
#ifndef RUNTIME_COMMUNICATION_H__
#define RUNTIME_COMMUNICATION_H__
#include "ctrl_runtime_messages.h"

int runtime_fd(unsigned int runtime_id);

int send_to_runtime(unsigned int runtime_id, struct ctrl_runtime_msg_hdr *hdr,
                    void *payload);
int runtime_communication_loop(int listen_port, char *output_file, int output_sock);

#endif
