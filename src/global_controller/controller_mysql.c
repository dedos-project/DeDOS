#include "controller_mysql.h"
#include "dfg.h"
#include "logging.h"
#include "stats.h"
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
            struct dfg_thread *thread = get_dfg_thread(rt, t);
            if (db_register_thread(thread->id, rt->id) != 0) {
                return -1;
            }

            int m;
            for (m = 0; m < thread->n_msus; ++m) {
                struct dfg_msu *msu = thread->msus[m];
                if (db_register_msu(msu, thread->id, rt->id) != 0) {
                    return -1;
                }

                if (db_register_timeseries(msu, thread->id, rt->id) != 0) {
                    return -1;
                }
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
             "select * from Threads where thread_id = (%d) and runtime_id = (%d)",
             thread_id, runtime_id);

    snprintf(insert_query, MAX_REQ_LEN,
             "insert into Threads (thread_id, runtime_id) values (%d, %d)",
             thread_id, runtime_id);

    return db_check_and_register(check_query, insert_query, element, thread_id);
}

/**
 * Register an MSU in the DB. Does nothing if MSU already exists
 * @param struct dfg_msu *msu
 * @param int thread_id
 * @param int runtime_id
 * @return: 0 on success
 */
int db_register_msu(struct dfg_msu *msu, int thread_id, int runtime_id) {
    char check_query[MAX_REQ_LEN];
    char insert_query[MAX_REQ_LEN];
    char select_thread_id[MAX_REQ_LEN];
    const char *element = "MSU";

    snprintf(check_query, MAX_REQ_LEN,
             "select * from Msus where msu_id = (%d)",
             msu->id);

    snprintf(select_thread_id, MAX_REQ_LEN,
             "select id from Threads where thread_id = (%d) and runtime_id = (%d)",
             thread_id, runtime_id);

    snprintf(insert_query, MAX_REQ_LEN,
             "insert into Msus (msu_id, msu_type_id, thread_id) values (%d, %d, (%s))",
             msu->id, msu->type->id, select_thread_id);

    return db_check_and_register(check_query, insert_query, element, msu->id);
}

/**
 * Register timseries for an MSU in the DB. Does nothing if timeserie already exists
 * @param struct dfg_msu *msu
 * @param int thread_id
 * @param int runtime_id
 * @return: 0 on success
 */
int db_register_timeseries(struct dfg_msu *msu, int thread_id, int runtime_id) {
    int i;
    for (i = 0; i < N_REPORTED_STAT_TYPES; ++i) {
        char check_query[MAX_REQ_LEN];
        char insert_query[MAX_REQ_LEN];
        char select_thread_id[MAX_REQ_LEN];
        char select_msu_id[MAX_REQ_LEN];
        char select_stat_id[MAX_REQ_LEN];
        const char *element = "timeserie";

        snprintf(check_query, MAX_REQ_LEN,
                 "select * from Timeseries where "
                 "msu_id = (select id from Msus where msu_id = (%d)) "
                 "and statistic_id = (select id from Statistics where stat_id = (%d))",
                 msu->id, reported_stat_types[i].id);

        snprintf(select_stat_id, MAX_REQ_LEN,
                 "select id from Statistics where name = ('%s')", reported_stat_types[i].name);

        snprintf(select_msu_id, MAX_REQ_LEN,
                 "select id from Msus where msu_id = (%d)", msu->id);

        snprintf(select_thread_id, MAX_REQ_LEN,
                 "select id from Threads where thread_id = (%d) and runtime_id = (%d)",
                 thread_id, runtime_id);

        snprintf(insert_query, MAX_REQ_LEN,
                 "insert into Timeseries (runtime_id, statistic_id, thread_id, msu_id) "
                 "values (%d, (%s), (%s), (%s))",
                 runtime_id, select_stat_id, select_thread_id, select_msu_id);

        if (db_check_and_register(check_query, insert_query, element, msu->id) != 0) {
            return -1;
        }
    }

    return 0;
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
                    log_error("MySQL query (%s) failed: %s", insert_query, mysql_error(&mysql));
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

int db_insert_sample(struct timed_stat *input, struct stat_sample_hdr *input_hdr) {
    /* Find timeserie to insert data point first */
    char select_stat_id[MAX_REQ_LEN];
    snprintf(select_stat_id, MAX_REQ_LEN,
             "select id from Statistics where stat_id = (%d)", input_hdr->stat_id);

    char select_msu_id[MAX_REQ_LEN];
    snprintf(select_msu_id, MAX_REQ_LEN,
             "select id from Msus where msu_id = (%d)", input_hdr->item_id);

    char ts_query[MAX_REQ_LEN];
    snprintf(ts_query, MAX_REQ_LEN,
             "select id from Timeseries where msu_id = (%s) and statistic_id = (%s)",
             select_msu_id, select_stat_id);

    int i;
    for (i = 0; i < input_hdr->n_stats; ++i) {
        int query_len;
        char insert_query[MAX_REQ_LEN];
        query_len = snprintf(insert_query, MAX_REQ_LEN,
                             "insert into Points (timeseries_id, ts, val) values "
                             "((%s), %lu, %f)",
                             ts_query,
                             input[i].time.tv_sec + input[i].time.tv_nsec,
                             input[i].value);

        if (mysql_real_query(&mysql, insert_query, query_len)) {
            log_error("MySQL query (%s) failed: %s", insert_query, mysql_error(&mysql));
            return -1;
        } else {
            log_info("inserted data point for msu %d and stat %d",
                     input_hdr->item_id, input_hdr->stat_id);
            return 0;
        }
    }

    return 0;
}
