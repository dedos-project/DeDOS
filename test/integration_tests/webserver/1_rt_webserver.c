#include "dedos_testing.h"
#include "integration_test_utils.h"
#include "msu_type_list.h"
#include "rt_stats.h"
#include "local_files.h"

// Need to set the file size smaller for the database, or memcheck will take forever
#define MAX_FILE_SIZE 16384
#include "socket_msu.c"
#include "webserver/connection-handler.c"
#include "webserver/dbops.c"
#include "webserver/http_msu.c"
#include "webserver/http_parser.c"
#include "webserver/read_msu.c"
#include "webserver/regex.c"
#include "webserver/regex_msu.c"
#include "webserver/request_parser.c"
#include "webserver/socketops.c"
#include "webserver/sslops.c"
#include "webserver/write_msu.c"
#include "webserver/cache_msu.c"
#include "webserver/fileio_msu.c"
#include "webserver/httpops.c"

#include <fcntl.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define PORT 8081

int connect_to_webserver() {
    double n_fds;
    last_msu_stat(MSU_STAT1, 10, &n_fds);
    ck_assert_int_eq(n_fds, 0);

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ck_assert_int_ne(sock, -1);

    struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(PORT),
    };

    int rtn = inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr.s_addr);
    if (rtn != 1) {
        log_perror("Error converting to network address");
    }
    ck_assert_int_eq(rtn, 1);

    rtn = connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    if (rtn < 0) {
        log_perror("Error connecting");
    }
    ck_assert_int_eq(rtn, 0);
    log_info("Connected!");
    return sock;
}


/**
 * InitCTX - initialize the SSL engine and global SSL context
 */
SSL_CTX * InitCTX(void) {

    SSL_library_init();
    OpenSSL_add_all_algorithms();       /* Load cryptos, et.al. */
    SSL_load_error_strings();           /* Bring in and register error messages */
    const SSL_METHOD *method = TLSv1_2_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);          /* Create new context */

    ck_assert_ptr_ne(ctx, NULL);
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3);
    init_ssl_locks();
    log_debug("Initialized the global context\n");
    return ctx;
}

SSL *ssl;

int ssl_start_connect_to_webserver() {
    SSL_CTX *ctx = InitCTX();
    ssl = SSL_new(ctx);
    int fd = connect_to_webserver();
    fcntl(fd, F_SETFL, O_NONBLOCK);

    int rtn = SSL_set_fd(ssl, fd);
    ck_assert_int_ne(rtn, 0);
    rtn = SSL_connect(ssl);
    if (rtn == 1)
        return fd;
    int err = SSL_get_error(ssl, rtn);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        log_info("Got want read/write");
        return fd;
    }
    ERR_print_errors_fp(stderr);
    return -1;
}

int ssl_fully_connect_to_webserver() {
    int fd = ssl_start_connect_to_webserver();
    ck_assert_int_gt(fd, 0);
    int rtn;
    long err = 0;
    do {
        rtn = SSL_connect(ssl);
        if (rtn != 1) {
            err = SSL_get_error(ssl, rtn);
            if (err != SSL_ERROR_WANT_WRITE && err != SSL_ERROR_WANT_READ) {
                const char* UNUSED error_str = ERR_error_string(err, NULL);
                log_error("SSL ERROR!: %d, %s", rtn, error_str);
            }
        }
    } while (rtn != 1 && (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE));
    while ((err = ERR_get_error()) != 0) {
        const char*  error_str = ERR_error_string(err, NULL);
        log_error("SSL ERROR!: %ld, %s", err, error_str);
    }

    ck_assert_int_eq(rtn, 1);
    log_info("Fully connected!");
    return fd;
}

void make_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, NULL);
    flags &= ~O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}

int write_full_http_request() {
    mark_point();
    int fd = ssl_fully_connect_to_webserver();
    make_nonblock(fd);

    char buffer[] =  "GET /index.html HTTP/1.1\r\nHost: dedos\r\nConnection: close\r\nUser-Agent: dedos\r\n\r\n";
    int rtn = SSL_write(ssl, buffer, strlen(buffer));
    ck_assert_int_eq(rtn,strlen(buffer));
    return fd;
}

int write_partial_http_request() {
    int fd = ssl_fully_connect_to_webserver();
    make_nonblock(fd);

    char buffer[] =  "GET /index.html HTTP/1.1\r\nHost: dedos\r\nConnection: close\r\nUser-Agent: dedos\r\n";
    int rtn = SSL_write(ssl, buffer, strlen(buffer));
    ck_assert_int_eq(rtn,strlen(buffer));
    return fd;
}

int write_partial_http_then_close() {
    int fd = write_partial_http_request();
    close(fd);
    return 0;
}

int write_and_read_regex_request() {
    int fd = ssl_fully_connect_to_webserver();
    make_nonblock(fd);

    char buffer[] =  "GET /?regex=abc HTTP/1.1\r\nHost: dedos\r\nConnection: close\r\nUser-Agent: dedos\r\n\r\n";
    int rtn = SSL_write(ssl, buffer, strlen(buffer));
    ck_assert_int_eq(rtn, strlen(buffer));
    char buff_out[1024];
    rtn = SSL_read(ssl, buff_out, 1024);
    ck_assert_int_gt(rtn, 0);
    return fd;
}

int write_and_read_http() {
    int fd = write_full_http_request();
    char buffer[1024];
    int rtn = SSL_read(ssl, buffer, 1024);
    ck_assert_int_gt(rtn, 0);
    return fd;
}

#define MANY_REQUESTS 100

int send_many_requests() {
    for (int i=0; i < MANY_REQUESTS; i++) {
        int fd = write_and_read_http();
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(fd);
        usleep(1000);
    }
    return 0;
}

int write_full_http_file_request(const char *file) {
    int fd = ssl_fully_connect_to_webserver();
    make_nonblock(fd);

    char buffer[256];
    int length = snprintf(buffer, 255,
                         "GET %s HTTP/1.1\r\nHost: dedos\r\nConnection: close\r\nUser-Agent: dedos\r\n\r\n",
                         file);

    int rtn = SSL_write(ssl, buffer, length);
    ck_assert_int_eq(rtn, length);
    return fd;
}

int write_and_read_http_file(const char *file) {
    int fd = write_full_http_file_request(file);
    char buffer[MAX_BODY_LEN + MAX_HEADER_LEN + 7]; // 3 carriage returns and null byte
    int rtn = SSL_read(ssl, buffer, 1024);
    ck_assert_int_gt(rtn, 0);
    return fd;
}

int write_and_read_files(const char **files, int n) {
    for (int i = 0; i < n; i++) {
        int fd = write_and_read_http_file(files[i]);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(fd);
        usleep(1000);
    }
    return 0;
}

int one_file_cache_test() {
    const char * files[] = {
            "/penn_3654.png",
            "/penn_3654.png"
    };
    write_and_read_files(files, 2);
    return 0;
}

int two_file_cache_test() {
    const char * files[] = {
            "/penn_1595.jpg",
            "/penn_2166.png",
            "/penn_1595.jpg",
            "/penn_2166.png"
    };
    write_and_read_files(files, 4);
    return 0;
}

int occupancy_limit_cache_test() {
    const char * files[] = {
            "/penn_4013.gif",
            "/penn_4013.gif"
    };
    write_and_read_files(files, 2);
    return 0;
}

int size_eviction_cache_test() {
    const char * files[] = {
            "/penn_3654.png",
            "/penn_2166.png",
            "/penn_2166.png",
            "/penn_3654.png"
    };
    write_and_read_files(files, 4);
    return 0;
}

int file_count_eviction_cache_test() {
    const char * files[] = {
            "/penn_2166.png",
            "/index.html",
            "/penn_1595.jpg",
            "/index.html",
            "/penn_2166.png",
    };
    write_and_read_files(files, 5);
    return 0;
}


START_DEDOS_INTEGRATION_TEST(connect_to_webserver) {
    double n_fds;
	last_msu_stat(MSU_STAT1, 10, &n_fds);
    ck_assert_int_eq(n_fds, 1);
    int n_states = type_num_states(&WEBSERVER_READ_MSU_TYPE);
    ck_assert_int_eq(n_states, 0);
} END_DEDOS_INTEGRATION_TEST()

START_DEDOS_INTEGRATION_TEST(ssl_start_connect_to_webserver) {
    double n_fds;
	last_msu_stat(MSU_STAT1, 10, &n_fds);
    ck_assert_int_eq(n_fds, 1);

    int n_states = type_num_states(&WEBSERVER_READ_MSU_TYPE);
    ck_assert_int_eq(n_states, 1);
} END_DEDOS_INTEGRATION_TEST()

START_DEDOS_INTEGRATION_TEST(ssl_fully_connect_to_webserver) {
    double n_fds;
	last_msu_stat(MSU_STAT1, 10, &n_fds);
    ck_assert_int_eq(n_fds, 1);

    int n_states = type_num_states(&WEBSERVER_READ_MSU_TYPE);
    ck_assert_int_eq(n_states, 1);
} END_DEDOS_INTEGRATION_TEST()

START_DEDOS_INTEGRATION_TEST(write_full_http_request) {
    double n_fds;
	last_msu_stat(MSU_STAT1, 10, &n_fds);
    ck_assert_int_eq(n_fds, 0);

    int n_states = type_num_states(&WEBSERVER_READ_MSU_TYPE);
    ck_assert_int_eq(n_states, 0);
} END_DEDOS_INTEGRATION_TEST()

START_DEDOS_INTEGRATION_TEST(write_partial_http_request) {
    double n_fds;
	last_msu_stat(MSU_STAT1, 10, &n_fds);
    ck_assert_int_eq(n_fds, 1);

    int n_http_states = type_num_states(&WEBSERVER_HTTP_MSU_TYPE);
    ck_assert_int_eq(n_http_states, 1);

    int n_states = type_num_states(&WEBSERVER_READ_MSU_TYPE);
    ck_assert_int_eq(n_states, 1);
} END_DEDOS_INTEGRATION_TEST()

START_DEDOS_INTEGRATION_TEST(write_partial_http_then_close) {
    double n_fds;
	last_msu_stat(MSU_STAT1, 10, &n_fds);
    ck_assert_int_eq(n_fds, 0);

    int n_http_states = type_num_states(&WEBSERVER_HTTP_MSU_TYPE);
    ck_assert_int_eq(n_http_states, 0);

    int n_states = type_num_states(&WEBSERVER_READ_MSU_TYPE);
    ck_assert_int_eq(n_states, 0);
} END_DEDOS_INTEGRATION_TEST()

START_DEDOS_INTEGRATION_TEST(write_and_read_regex_request) {
    double n_fds;
	last_msu_stat(MSU_STAT1, 10, &n_fds);
    ck_assert_int_eq(n_fds, 0);
} END_DEDOS_INTEGRATION_TEST()

START_DEDOS_INTEGRATION_TEST(write_and_read_http) {
    double n_fds;
	last_msu_stat(MSU_STAT1, 10, &n_fds);
    ck_assert_int_eq(n_fds, 0);

    int n_states = type_num_states(&WEBSERVER_READ_MSU_TYPE);
    ck_assert_int_eq(n_states, 0);
    n_states = type_num_states(&WEBSERVER_WRITE_MSU_TYPE);
    ck_assert_int_eq(n_states, 0);
} END_DEDOS_INTEGRATION_TEST()

START_DEDOS_INTEGRATION_TEST(send_many_requests) {
    double n_fds;
	last_msu_stat(MSU_STAT1, 10, &n_fds);
    ck_assert_int_eq(n_fds, 0);

    int n_states = type_num_states(&WEBSERVER_READ_MSU_TYPE);
    ck_assert_int_eq(n_states, 0);
    n_states = type_num_states(&WEBSERVER_WRITE_MSU_TYPE);
    ck_assert_int_eq(n_states, 0);
} END_DEDOS_INTEGRATION_TEST()

START_DEDOS_INTEGRATION_TEST(one_file_cache_test) {
    double hits, misses, evicts;
    last_msu_stat(CACHE_HIT_STAT, 14, &hits);
    last_msu_stat(CACHE_MISS_STAT, 14, &misses);
    last_msu_stat(CACHE_EVICT_STAT, 14, &evicts);
    ck_assert_int_eq(hits, 1);
    ck_assert_int_eq(misses, 1);
    ck_assert_int_eq(evicts, 0);

    struct ws_cache_state *cache = (struct ws_cache_state *) cache_instance->msu_state;
    ck_assert_int_eq(cache->file_count, 1);
    ck_assert_int_eq(cache->byte_size, 3654);
} END_DEDOS_INTEGRATION_TEST()

START_DEDOS_INTEGRATION_TEST(two_file_cache_test) {
    double hits, misses, evicts;
    last_msu_stat(CACHE_HIT_STAT, 14, &hits);
    last_msu_stat(CACHE_MISS_STAT, 14, &misses);
    last_msu_stat(CACHE_EVICT_STAT, 14, &evicts);
    ck_assert_int_eq(hits, 2);
    ck_assert_int_eq(misses, 2);
    ck_assert_int_eq(evicts, 0);

    struct ws_cache_state *cache = (struct ws_cache_state *) cache_instance->msu_state;
    ck_assert_int_eq(cache->file_count, 2);
    ck_assert_int_eq(cache->byte_size, 3761);
} END_DEDOS_INTEGRATION_TEST()

START_DEDOS_INTEGRATION_TEST(occupancy_limit_cache_test) {
    double hits, misses, evicts;
    last_msu_stat(CACHE_HIT_STAT, 14, &hits);
    last_msu_stat(CACHE_MISS_STAT, 14, &misses);
    last_msu_stat(CACHE_EVICT_STAT, 14, &evicts);
    ck_assert_int_eq(hits, 0);
    ck_assert_int_eq(misses, 2);
    ck_assert_int_eq(evicts, 0);

    struct ws_cache_state *cache = (struct ws_cache_state *) cache_instance->msu_state;
    ck_assert_int_eq(cache->file_count, 0);
    ck_assert_int_eq(cache->byte_size, 0);
} END_DEDOS_INTEGRATION_TEST()

START_DEDOS_INTEGRATION_TEST(size_eviction_cache_test) {
    double hits, misses, evicts;
    last_msu_stat(CACHE_HIT_STAT, 14, &hits);
    last_msu_stat(CACHE_MISS_STAT, 14, &misses);
    last_msu_stat(CACHE_EVICT_STAT, 14, &evicts);
    ck_assert_int_eq(hits, 1);
    ck_assert_int_eq(misses, 3);
    ck_assert_int_eq(evicts, 2);

    struct ws_cache_state *cache = (struct ws_cache_state *) cache_instance->msu_state;
    ck_assert_int_eq(cache->file_count, 1);
    ck_assert_int_eq(cache->byte_size, 3654);
} END_DEDOS_INTEGRATION_TEST()

START_DEDOS_INTEGRATION_TEST(file_count_eviction_cache_test) {
    double hits, misses, evicts;
    last_msu_stat(CACHE_HIT_STAT, 14, &hits);
    last_msu_stat(CACHE_MISS_STAT, 14, &misses);
    last_msu_stat(CACHE_EVICT_STAT, 14, &evicts);
    ck_assert_int_eq(hits, 1);
    ck_assert_int_eq(misses, 4);
    ck_assert_int_eq(evicts, 2);

    struct ws_cache_state *cache = (struct ws_cache_state *) cache_instance->msu_state;
    ck_assert_int_eq(cache->file_count, 2);
    ck_assert_int_eq(cache->byte_size, 2260);
} END_DEDOS_INTEGRATION_TEST()


DEDOS_START_INTEGRATION_TEST_LIST("1_rt_webserver.json");
DEDOS_ADD_INTEGRATION_TEST_FN(connect_to_webserver)
DEDOS_ADD_INTEGRATION_TEST_FN(ssl_start_connect_to_webserver)
DEDOS_ADD_INTEGRATION_TEST_FN(ssl_fully_connect_to_webserver)
DEDOS_ADD_INTEGRATION_TEST_FN(write_full_http_request)
DEDOS_ADD_INTEGRATION_TEST_FN(write_partial_http_request)
DEDOS_ADD_INTEGRATION_TEST_FN(write_partial_http_then_close)
DEDOS_ADD_INTEGRATION_TEST_FN(write_and_read_regex_request)
DEDOS_ADD_INTEGRATION_TEST_FN(write_and_read_http)
DEDOS_ADD_INTEGRATION_TEST_FN(send_many_requests)
DEDOS_ADD_INTEGRATION_TEST_FN(one_file_cache_test)
DEDOS_ADD_INTEGRATION_TEST_FN(two_file_cache_test)
DEDOS_ADD_INTEGRATION_TEST_FN(occupancy_limit_cache_test)
DEDOS_ADD_INTEGRATION_TEST_FN(size_eviction_cache_test)
DEDOS_ADD_INTEGRATION_TEST_FN(file_count_eviction_cache_test)
DEDOS_END_INTEGRATION_TEST_LIST()
