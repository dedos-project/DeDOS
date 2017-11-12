#include "controller_mysql.h"
#include "dfg.h"
#include "logging.h"
#include "stdlib.h"
#include "mysql.h"

MYSQL mysql;

int db_init() {
    struct db_info *db = get_db_info();

    if (mysql_library_init(0, NULL, NULL)) {
        log_error("Could not initialize MySQL library");
        return -1;
    }

    char db_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &db->ip, db_ip, INET_ADDRSTRLEN);

    if (!mysql_real_connect(&mysql, db_ip, db->user, db->pwd, db->name, 0, NULL, 0)) {
        log_error("Could not connect to MySQL DB %s", mysql_error(&mysql));
        return -1;
    }

    return 1;
}

int db_terminate() {
    mysql_library_end();

    return 1;
}
