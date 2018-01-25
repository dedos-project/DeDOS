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

static pthread_mutex_t lock;
static MYSQL mysql;
bool mysql_initialized = false;

#define CHECK_SQL_INIT \
    if (!mysql_initialized) { \
        log(LOG_SQL,"MYSQL not initialized"); \
        return -1; \
    }

#define SQL_LOCK \
    pthread_mutex_lock(&lock)

#define SQL_UNLOCK \
    pthread_mutex_unlock(&lock)

#define MAX_REQ_LEN 4096

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
    pthread_mutex_init(&lock, 0);
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
                                  reported_stat_types[i].label) != 0) {
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

int db_register_rt_timeseries(int runtime_id) {
    CHECK_SQL_INIT;
    char check_query[MAX_REQ_LEN];
    char insert_query[MAX_REQ_LEN];
    for (int i=0; i < N_RUNTIME_STAT_TYPES; ++i) {
        snprintf(check_query, MAX_REQ_LEN,
                 "select * from Timeseries where "
                 "runtime_id = %d and statistic_id = %d",
                 runtime_id, runtime_stat_types[i].id);
        snprintf(insert_query, MAX_REQ_LEN,
                 "insert into Timeseries (statistic_id, runtime_id) "
                 "values ((%d), (%d))",
                 runtime_stat_types[i].id, runtime_id);
        if (db_check_and_register(check_query, insert_query, "rt_timeseries", runtime_id) != 0) {
            return -1;
        }
    }
    return 0;
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

    int rtn = db_check_and_register(check_query, insert_query, element, runtime_id);
    if (rtn < 0) {
        log_error("Error registering runtime %d", runtime_id);
        return -1;
    }

    rtn = db_register_rt_timeseries(runtime_id);
    if (rtn < 0) {
        log_error("Error registering runtime %d timeseries", runtime_id);
        return -1;
    }
    return 0;
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
    for (i = 0; i < N_MSU_STAT_TYPES; ++i) {
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
                 select_msu_pk, msu_stat_types[i].id);


        snprintf(insert_query, MAX_REQ_LEN,
                 "insert into Timeseries (statistic_id, msu_pk) "
                 "values ((%d), (%s))",
                 msu_stat_types[i].id, select_msu_pk);

        if (db_check_and_register(check_query, insert_query, element, msu_id) != 0) {
            return -1;
        }
    }
    return 0;
}

int db_set_rt_stat_limit(int runtime_id, enum stat_id stat_id, double limit) {
    char query[MAX_REQ_LEN];

    size_t qlen = snprintf(query, MAX_REQ_LEN,
             "UPDATE Timeseries SET max_limit = %f "
             "WHERE runtime_id = %d and statistic_id = %d",
             limit, runtime_id, stat_id);

    if (mysql_real_query(&mysql, query, qlen)) {
        log_error("Error making query %s: %s", query, mysql_error(&mysql));
        return -1;
    }
    return 0;
}



int db_register_thread_timeseries(int thread_id, int runtime_id) {
    CHECK_SQL_INIT;
    char check_query[MAX_REQ_LEN];
    char insert_query[MAX_REQ_LEN];
    char select_thread_pk[MAX_REQ_LEN];
    const char *element = "thread_timeseries";
    for (int i=0; i < N_THREAD_STAT_TYPES; ++i) {
        snprintf(select_thread_pk, MAX_REQ_LEN,
                 "select pk from Threads where thread_id = (%d) and runtime_id = (%d)",
                 thread_id, runtime_id);

        snprintf(check_query, MAX_REQ_LEN,
                 "select * from Timeseries where "
                 "thread_pk = (%s) "
                 "and statistic_id = (%d)",
                 select_thread_pk, thread_stat_types[i].id);

        snprintf(insert_query, MAX_REQ_LEN,
                "insert into Timeseries (statistic_id, thread_pk) "
                "values ((%d), (%s))",
                thread_stat_types[i].id, select_thread_pk);

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
    SQL_LOCK;
    int query_len;

    query_len = strlen(check_query);
    if (mysql_real_query(&mysql, check_query, query_len)) {
        log_error("MySQL query failed: %s\n %s", mysql_error(&mysql), check_query);
        SQL_UNLOCK;
        return -1;
    } else {
        MYSQL_RES *result = mysql_store_result(&mysql);
        if (!result) {
            log_error("Could not get result from MySQL query %s", mysql_error(&mysql));
            SQL_UNLOCK;
            return -1;
        } else {
            if (mysql_num_rows(result) == 0) {
                mysql_free_result(result);

                query_len = strlen(insert_query);
                if (mysql_real_query(&mysql, insert_query, query_len)) {
                    log_error("MySQL query (%s) failed: %s", insert_query, mysql_error(&mysql));
                    SQL_UNLOCK;
                    return -1;
                } else {
                    log(LOG_SQL, "registered element %s (id: %d) in DB", element, element_id);
                    SQL_UNLOCK;
                    return 0;
                }
            } else {
                log(LOG_SQL, "element %s (id: %d) is already registered in DB", element, element_id);
                mysql_free_result(result);
                SQL_UNLOCK;
                return 0;
            }
        }
    }
}

static int get_ts_query(char query[MAX_REQ_LEN], enum stat_id stat_id,
                        struct stat_referent *referent, unsigned int runtime_id) {
    char select_pk[MAX_REQ_LEN];
    switch (referent->type) {
        case MSU_STAT:
            snprintf(select_pk, MAX_REQ_LEN,
                     "select pk from Msus where msu_id = %u", referent->id);
            snprintf(query, MAX_REQ_LEN,
                    "select pk from Timeseries where msu_pk = (%s) and statistic_id = (%d)",
                    select_pk, stat_id);
            return 0;
        case THREAD_STAT:
            snprintf(select_pk, MAX_REQ_LEN,
                     "select pk from Threads where thread_id = %d and runtime_id = %u",
                     referent->id, runtime_id);
            snprintf(query, MAX_REQ_LEN,
                     "select pk from Timeseries where thread_pk = (%s) and statistic_id = (%d)",
                     select_pk, stat_id);
            return 0;
        case RT_STAT:
            snprintf(query, MAX_REQ_LEN,
                     "select pk from Timeseries where runtime_id = (%u) and statistic_id = (%d)",
                     runtime_id, stat_id);
            return 0;
        default:
            log_error("Cannot get ts query for stat type %d", stat_id);
            return -1;
    }
}
/**
 * Insert datapoint for a timseries in the DB.
 * @param input: pointer to timed_stat object
 * @param input_hdr: header of stat sample
 * @return: 0 on success
 */
int db_insert_sample(struct stat_sample *sample, unsigned int runtime_id) {
    CHECK_SQL_INIT;
    SQL_LOCK;
    /* Find timeserie to insert data point first */
    char ts_query[MAX_REQ_LEN];
    if (get_ts_query(ts_query, sample->stat_id, &sample->referent, runtime_id) != 0) {
        SQL_UNLOCK;
        return -1;
    }

    char query[MAX_REQ_LEN];
    unsigned long ts = (unsigned long) sample->start.tv_sec * (unsigned long)1e9 +
                       (unsigned long) sample->start.tv_nsec;
    size_t query_len = snprintf(query, MAX_REQ_LEN,
                                "insert into Points (timeseries_pk, ts) values "
                                "((%s), %lu)",
                                ts_query, ts);

    if (mysql_real_query(&mysql, query, query_len)) {
        log_error("MySQL query (%s) failed: %s", query, mysql_error(&mysql));
        SQL_UNLOCK;
        return -1;
    }

    char points_query[MAX_REQ_LEN];
    query_len = snprintf(points_query, MAX_REQ_LEN,
                         "select pk from Points where timeseries_pk = (%s) and ts = (%lu)",
                         ts_query, ts);

    if (mysql_real_query(&mysql, points_query, query_len)) {
        log_error("MySQL query (%s) failed: %s", query, mysql_error(&mysql));
        SQL_UNLOCK;
        return -1;
    }

    MYSQL_RES *result = mysql_store_result(&mysql);
    if (result == NULL) {
        log_error("Could not find result of points query after insertion");
        SQL_UNLOCK;
        return -1;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    if (!row) {
        log_error("Could not fetch row of resulting query");
        SQL_UNLOCK;
        return -1;
    }

    char *points_pk = row[0];

    char values[MAX_REQ_LEN];
    size_t values_size = 0;
    double so_far = 0;
    double total_size = sample->total_samples;
    for (int i=0; i < sample->n_bins; i++) {
        so_far += sample->bin_size[i];
        values_size += snprintf(values + values_size, MAX_REQ_LEN - values_size,
                                "\n(%lf, %lf, %u, %f, %s),",
                                sample->bin_edges[i],
                                sample->bin_edges[i+1],
                                sample->bin_size[i],
                                100.0 * so_far / total_size,
                                points_pk);
    }
    values[values_size - 1] = '\0';
    query_len = snprintf(query, MAX_REQ_LEN,
                         "Insert into Bins (low, high, size, percentile, points_pk) values %s",
                         values);
    if (mysql_real_query(&mysql, query, query_len)) {
        log_error("MySQL query (%s) failed: %s", query, mysql_error(&mysql));
        SQL_UNLOCK;
        return -1;
    }
    mysql_free_result(result);
    log(LOG_MYSQL, "Inserted data point for stat %d (%s)", sample->stat_id, points_query);
    SQL_UNLOCK;
    return 0;
}

int db_insert_samples(struct stat_sample *samples, int n_samples, unsigned int runtime_id) {
    for (int i=0; i < n_samples; i++) {
        if (db_insert_sample(&samples[i], runtime_id) != 0) {
            return -1;
        }
    }
    return 0;
}


