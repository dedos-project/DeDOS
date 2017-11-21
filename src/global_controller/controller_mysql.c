#include "controller_mysql.h"
#include "dfg.h"
#include "logging.h"
#include "stats.h"
#include "stdlib.h"
#include "local_files.h"
#include "controller_dfg.h"

#include <mysql.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static MYSQL mysql;
bool mysql_initialized = false;

#define CHECK_SQL_INIT \
    if (!mysql_initialized) { \
        log(LOG_SQL,"MYSQL not initialized"); \
        return -1; \
    }


#define MAX_REQ_LEN 1024

#define MAX_INIT_CONTENTS_SIZE 8192

int db_register_msu_type(int msu_type_id, char *name);
int db_register_statistic(int stat_id, char *name);
int db_register_thread_stats(int thread_id, int runtime_id);

static int split_exec_cmd(char *cmd) {
    char req[MAX_REQ_LEN];

    char *delimed = strtok(cmd, ";");
    while (delimed != NULL && strlen(delimed) > 1) {
        strncpy(req, delimed, MAX_REQ_LEN);
        int len = strlen(delimed);
        req[len] = ';';
        req[len+1] = '\0';

        int status = mysql_query(&mysql, req);
        if (status) {
            log_error("Could not execute mysql query %s. Error: %s",
                      req, mysql_error(&mysql));
            return -1;
        }
        delimed = strtok(NULL, ";");
    }
    return 0;
}


static int db_clear() {
    char db_init_file[256];
    get_local_file(db_init_file, "schema.sql");
    char contents[MAX_INIT_CONTENTS_SIZE];

    int fd = open(db_init_file, O_RDONLY);
    if (fd == -1) {
        log_error("Error opening schema file %s", db_init_file);
        return -1;
    }

    ssize_t bytes_read = read(fd, contents, MAX_INIT_CONTENTS_SIZE);
    close(fd);
    if (bytes_read == MAX_INIT_CONTENTS_SIZE) {
        log_error("Contents of %s too large", db_init_file);
        return -1;
    }

    contents[bytes_read] = '\0';

    int status = split_exec_cmd(contents);
    if (status) {
        log_error("Could not execute mysql query in %s", db_init_file);
        return -1;
    }
    log_info("Initialized cleared db");
    return 0;
}

/**
 * Initialize the MySQL client library, and connect to the server
 * Also init tables for running system
 * @param: none
 * @return: 0 on success
 */
int db_init(int clear) {
    struct db_info *db = get_db_info();

    if (mysql_library_init(0, NULL, NULL)) {
        log_error("Could not initialize MySQL library");
        return -1;
    }

    char db_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &db->ip, db_ip, INET_ADDRSTRLEN);

    if (!mysql_real_connect(&mysql, db_ip, db->user, db->pwd, db->name, 0, NULL,
                            CLIENT_MULTI_STATEMENTS)) {
        log_error("Could not connect to MySQL DB %s", mysql_error(&mysql));
        return -1;
    }

    if (clear) {
        int rtn = db_clear();
        if (rtn < 0) {
            log_error("Could not clear Database");
            return -1;
        }
    }
    mysql_initialized = true;

    for (int i = 0; i < N_REPORTED_STAT_TYPES; ++i) {
        if (db_register_statistic(reported_stat_types[i].id,
                                  reported_stat_types[i].name) != 0) {
            return -1;
        }
    }

    struct dedos_dfg *dfg = get_dfg();

    for (int i=0; i < dfg->n_msu_types; ++i) {
        struct dfg_msu_type *type = dfg->msu_types[i];
        if (db_register_msu_type(type->id, type->name) != 0) {
            return -1;
        }
    }

    int r;
    for (r = 1; r <= get_dfg_n_runtimes(); ++r) {
        struct dfg_runtime *rt = get_dfg_runtime(r);
        if (db_register_runtime(rt->id) != 0) {
            return -1;
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
    mysql_initialized = false;
    return 1;
}

int db_register_statistic(int stat_id, char *name) {
    CHECK_SQL_INIT;
    char check_query[MAX_REQ_LEN];
    char insert_query[MAX_REQ_LEN];
    const char *element = "stattype";

    snprintf(check_query, MAX_REQ_LEN,
             "select * from Statistics where id = (%d)", stat_id);

    snprintf(insert_query, MAX_REQ_LEN,
             "insert into Statistics (id, name) values (%d, '%s')",
             stat_id, name);

    return db_check_and_register(check_query, insert_query, element, stat_id);
}

int db_register_msu_type(int msu_type_id, char *name) {
    CHECK_SQL_INIT;
    char check_query[MAX_REQ_LEN];
    char insert_query[MAX_REQ_LEN];
    const char *element = "msutype";

    snprintf(check_query, MAX_REQ_LEN,
             "select * from MsuTypes where id = (%d)", msu_type_id);

    snprintf(insert_query, MAX_REQ_LEN,
             "insert into MsuTypes (id, name) values (%d, '%s')", msu_type_id, name);

    return db_check_and_register(check_query, insert_query, element, msu_type_id);
}

/**
 * Register a runtime in the DB. Does nothing if runtime id already exists
 * @param int runtime_id: the runtime ID
 * @return: 0 on success
 */
int db_register_runtime(int runtime_id) {
    CHECK_SQL_INIT;
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
    CHECK_SQL_INIT;
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
int db_register_msu(int msu_id, int msu_type_id, int thread_id, int runtime_id) {
    CHECK_SQL_INIT;
    char check_query[MAX_REQ_LEN];
    char insert_query[MAX_REQ_LEN];
    char select_thread_id[MAX_REQ_LEN];
    const char *element = "MSU";

    snprintf(check_query, MAX_REQ_LEN,
             "select * from Msus where msu_id = (%d)",
             msu_id);

    snprintf(select_thread_id, MAX_REQ_LEN,
             "select pk from Threads where thread_id = (%d) and runtime_id = (%d)",
             thread_id, runtime_id);

    snprintf(insert_query, MAX_REQ_LEN,
             "insert into Msus (msu_id, msu_type_id, thread_pk) values (%d, %d, (%s))",
             msu_id, msu_type_id, select_thread_id);

    return db_check_and_register(check_query, insert_query, element, msu_id);
}

/**
 * Register timseries for an MSU in the DB. Does nothing if timeserie already exists
 * @return: 0 on success
 */
int db_register_msu_timeseries(int msu_id) {
    CHECK_SQL_INIT;
    int i;
    for (i = 0; i < N_REPORTED_MSU_STAT_TYPES; ++i) {
        char check_query[MAX_REQ_LEN];
        char insert_query[MAX_REQ_LEN];
        char select_msu_pk[MAX_REQ_LEN];
        const char *element = "timeserie";

        snprintf(select_msu_pk, MAX_REQ_LEN,
                 "select pk from Msus where msu_id = (%d)", msu_id);

        snprintf(check_query, MAX_REQ_LEN,
                 "select * from Timeseries where "
                 "msu_pk = (%s) "
                 "and statistic_id = (%d)",
                 select_msu_pk, reported_msu_stat_types[i].id);


        snprintf(insert_query, MAX_REQ_LEN,
                 "insert into Timeseries (statistic_id, msu_pk) "
                 "values ((%d), (%s))",
                 reported_msu_stat_types[i].id, select_msu_pk);

        if (db_check_and_register(check_query, insert_query, element, msu_id) != 0) {
            return -1;
        }
    }
    return 0;
}

int db_register_thread_timeseries(int thread_id, int runtime_id) {
    CHECK_SQL_INIT;
    char check_query[MAX_REQ_LEN];
    char insert_query[MAX_REQ_LEN];
    char select_thread_pk[MAX_REQ_LEN];
    const char *element = "thread_timeseries";
    for (int i=0; i < N_REPORTED_THREAD_STAT_TYPES; ++i) {
        snprintf(select_thread_pk, MAX_REQ_LEN,
                 "select pk from Threads where thread_id = (%d) and runtime_id = (%d)",
                 thread_id, runtime_id);

        snprintf(check_query, MAX_REQ_LEN,
                 "select * from Timeseries where "
                 "thread_pk = (%s) "
                 "and statistic_id = (%d)",
                 select_thread_pk, reported_thread_stat_types[i].id);

        snprintf(insert_query, MAX_REQ_LEN,
                "insert into Timeseries (statistic_id, thread_pk) "
                "values ((%d), (%s))",
                reported_thread_stat_types[i].id, select_thread_pk);

        if (db_check_and_register(check_query, insert_query, element, thread_id) != 0) {
            return -1;
        }
    }
    return 0;
}



int db_register_thread_stats(int thread_id, int runtime_id) {
    CHECK_SQL_INIT;
    if (db_register_thread(thread_id, runtime_id) != 0) {
        return -1;
    }
    if (db_register_thread_timeseries(thread_id, runtime_id) != 0) {
        return -1;
    }
    return 0;
}

int db_register_msu_stats(int msu_id, int msu_type_id, int thread_id, int runtime_id) {
    CHECK_SQL_INIT;

    if (db_register_msu(msu_id, msu_type_id, thread_id, runtime_id) != 0) {
        return -1;
    }
    if (db_register_msu_timeseries(msu_id) != 0) {
        return -1;
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
    CHECK_SQL_INIT;
    int query_len;

    query_len = strlen(check_query);
    if (mysql_real_query(&mysql, check_query, query_len)) {
        log_error("MySQL query failed: %s\n %s", mysql_error(&mysql), check_query);
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
                    log(LOG_SQL, "registered element %s (id: %d) in DB", element, element_id);
                    return 0;
                }
            } else {
                log(LOG_SQL, "element %s (id: %d) is already registered in DB", element, element_id);
                mysql_free_result(result);
                return 0;
            }
        }
    }
}

static int get_ts_query(char query[MAX_REQ_LEN], enum stat_id stat_id, int item_id, int runtime_id) {
    char select_pk[MAX_REQ_LEN];
    if (is_thread_stat(stat_id)) {
        snprintf(select_pk, MAX_REQ_LEN,
                "select pk from Threads where thread_id = %d and runtime_id = %d",
                item_id, runtime_id);
        snprintf(query, MAX_REQ_LEN,
                "select pk from Timeseries where thread_pk = (%s) and statistic_id = (%d)",
                select_pk, stat_id);
        return 0;
    } else if (is_msu_stat(stat_id)) {
        snprintf(select_pk, MAX_REQ_LEN,
                "select pk from Msus where msu_id = %d", item_id);
        snprintf(query, MAX_REQ_LEN,
                "select pk from Timeseries where msu_pk = (%s) and statistic_id = (%d)",
                select_pk, stat_id);
        return 0;
    } else {
        log_error("Cannot get timestamp query for stat type %d", stat_id);
        return -1;
    }
}

/**
 * Insert datapoint for a timseries in the DB.
 * @param input: pointer to timed_stat object
 * @param input_hdr: header of stat sample
 * @return: 0 on success
 */
int db_insert_sample(struct timed_stat *input, struct stat_sample_hdr *input_hdr, int runtime_id) {
    CHECK_SQL_INIT;
    /* Find timeserie to insert data point first */
    char ts_query[MAX_REQ_LEN];
    if (get_ts_query(ts_query, input_hdr->stat_id, input_hdr->item_id, runtime_id) != 0) {
        return -1;
    }
    int i;
    for (i = 0; i < input_hdr->n_stats; ++i) {
        if (input[i].time.tv_sec < 0) {
            continue;
        }
        int query_len;
        char insert_query[MAX_REQ_LEN];
        query_len = snprintf(insert_query, MAX_REQ_LEN,
                             "insert into Points (timeseries_pk, ts, val) values "
                             "((%s), %lu, %Lf)",
                             ts_query,
                             (unsigned long) input[i].time.tv_sec * (unsigned long)1e9 + (unsigned long)input[i].time.tv_nsec,
                             input[i].value);

        if (mysql_real_query(&mysql, insert_query, query_len)) {
            log_error("MySQL query (%s) failed: %s", insert_query, mysql_error(&mysql));
            return -1;
        } else {
            log(LOG_SQL, "inserted data point for msu %d and stat %d",
                input_hdr->item_id, input_hdr->stat_id);
            return 0;
        }
    }

    return 0;
}
