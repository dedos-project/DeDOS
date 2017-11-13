#include "controller_mysql.h"
#include "dfg.h"
#include "logging.h"
#include "stdlib.h"
#include "mysql.h"

MYSQL mysql;

/**
 * Initialize the MySQL client library, and connect to the server
 * Also init tables for running system
 * @param: none
 * @return: 0 on success
 */
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

    int r;
    for (r = 1; r <= get_dfg_n_runtimes(); ++r) {
        struct dfg_runtime *rt = get_dfg_runtime(r);
        if (db_register_runtime(rt->id) != 0) {
            return -1;
        }

        int n_threads = rt->n_pinned_threads + rt->n_unpinned_threads;
        int t;
        for (t = 1; t <= n_threads; ++t) {
            if (db_register_thread(t, rt->id) != 0) {
                return -1;
            }
        }

    }

    return 0;
}

/**
 * Destroy the MySQL client environment
 * @param: none
 * @return: 0 on success
 */
int db_terminate() {
    mysql_library_end();

    return 1;
}

/**
 * Register a runtime in the DB. Does nothing if runtime id already exists
 * @param int runtime_id: the runtime ID
 * @return: 0 on success
 */
int db_register_runtime(int runtime_id) {
    char check_query[MAX_REQ_LEN];
    char insert_query[MAX_REQ_LEN];
    const char *element = "runtime";

    snprintf(check_query, MAX_REQ_LEN,
             "select * from Runtimes where id = (%d)",
             runtime_id);

    snprintf(insert_query, MAX_REQ_LEN,
             "insert into Runtimes (id) values (%d)",
             runtime_id);

    return db_check_and_register(check_query, insert_query, element, runtime_id);
}

/**
 * Register a runtime's thread in the DB. Does nothing if thread already exists
 * @param int thread_id
 * @param int runtime_id
 * @return: 0 on success
 */
int db_register_thread(int thread_id, int runtime_id) {
    char check_query[MAX_REQ_LEN];
    char insert_query[MAX_REQ_LEN];
    const char *element = "thread";

    snprintf(check_query, MAX_REQ_LEN,
             "select * from Threads where num = (%d) AND runtime_id = (%d)",
             thread_id, runtime_id);

    snprintf(insert_query, MAX_REQ_LEN,
             "insert into Threads (num, runtime_id) values (%d, %d)",
             thread_id, runtime_id);

    return db_check_and_register(check_query, insert_query, element, thread_id);
}

/**
 * Register an element in the DB. Does nothing if element already exists
 * @param const char *check_query: query to check existence of element
 * @param const char *insert_query: query to insert element
 * @param const char *element: element's type
 * @param int element_id
 * @return: 0 on success
 */
int db_check_and_register(const char *check_query, const char *insert_query,
                          const char *element, int element_id) {
    int query_len;

    query_len = strlen(check_query);
    if (mysql_real_query(&mysql, check_query, query_len)) {
        log_error("MySQL query failed: %s", mysql_error(&mysql));
        return -1;
    } else {
        MYSQL_RES *result = mysql_store_result(&mysql);
        if (!result) {
            log_error("Could not get result from MySQL query %s", mysql_error(&mysql));
            return -1;
        } else {
            if (mysql_num_rows(result) == 0) {
                mysql_free_result(result);

                query_len = strlen(insert_query);
                if (mysql_real_query(&mysql, insert_query, query_len)) {
                    log_error("MySQL query failed: %s", mysql_error(&mysql));
                    return -1;
                } else {
                    log_info("registered element %s (id: %d) in DB", element, element_id);
                    return 0;
                }
            } else {
                log_info("element %s (id: %d) is already registered in DB", element, element_id);
                mysql_free_result(result);
                return 0;
            }
        }
    }
}

/*
int db_register_msu(int thread_id) {
}
int db_register_timeserie(int thread_id) {
}
*/
