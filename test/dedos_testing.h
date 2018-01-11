#ifndef DEDOS_TESTING_H_
#define DEDOS_TESTING_H_

#include <unistd.h>
#include <check.h>
#include <libgen.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdlib.h>

#define START_DEDOS_TEST(name) \
    START_TEST(name) { \
        fprintf(LOG_FD, ANSI_COLOR_BLUE "******** " #name " ******** \n" ANSI_COLOR_RESET);

#define END_DEDOS_TEST \
    } END_TEST


#ifndef LOG_ALL
#define LOG_ALL
#endif
#ifndef LOG_CUSTOM
#define LOG_CUSTOM
#endif
#include "logging.h"

#define DEDOS_START_TEST_LIST(name) \
    int main(int argc, char **argv) { \
        Suite *s = suite_create(name); \
        TCase *tc = tcase_create(name); \

#define DEDOS_ADD_TEST_RESOURCE(var, file) \
        sprintf(var, "%s/%s", dirname(argv[0]), file); \
        if (access( var, F_OK ) == -1) { \
            log_warn("File %s does not exist!", var); \
        } \

#define DEDOS_ADD_TEST_FN(fn) \
        tcase_add_test(tc, fn);

#define DEDOS_END_TEST_LIST() \
        suite_add_tcase(s, tc); \
        SRunner *sr;\
        sr = srunner_create(s); \
        srunner_run_all(sr, CK_NORMAL); \
        int num_failed = srunner_ntests_failed(sr); \
        srunner_free(sr); \
        return (num_failed == 0) ? 0 : -1; \
    }


int init_test_listening_socket(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1) {
        log_perror("Failed to create socket");
        return -1;
    }
    int val = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val)) < 0) {
        log_perror("Error setting SO_REUSEPORT on socket");
        return -1;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0) {
        log_perror("Error setting SO_REUSEADDR on socket");
        return -1;
    }

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        log_perror("Failed to bind to port %d", port);
        return -1;
    }

    int rtn = listen(sock, 10);
    if (rtn < 0) {
        log_perror("Error starting listening socket");
        return -1;
    }
    log(LOG_TEST, "Started listen socket on port %d", port);
    return sock;
}

int init_sock_pair(int *other_end) {
    int fds[2];
    int rtn = socketpair(PF_LOCAL, SOCK_STREAM, 0, fds);
    if (rtn < 0) {
        log_perror("Error creating socket pair");
        return -1;
    }
    *other_end = fds[0];
    return fds[1];
}

#endif
