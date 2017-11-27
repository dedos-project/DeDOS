/*
START OF LICENSE STUB
    DeDOS: Declarative Dispersion-Oriented Software
    Copyright (C) 2017 University of Pennsylvania

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
#ifndef DB_OPS_H
#define DB_OPS_H

enum db_status {
    DB_NO_CONNECTION,
    DB_CONNECTING,
    DB_SEND,
    DB_RECV,
    DB_DONE,
    DB_ERR
};

struct db_state {
    int db_fd;
    char db_req[16];
    void *data;
    enum db_status status;
};

void init_db(char *ip, int port, int max_load);
void *allocate_db_memory();
void free_db_memory(void *memory);
int query_db(struct db_state *state);
#endif
