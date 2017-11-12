#ifndef CONTROLLER_MYSQL_H
#define CONTROLLER_MYSQL_H

#include "mysql.h"

extern MYSQL mysql;

int db_init();
int db_terminate();

#endif
