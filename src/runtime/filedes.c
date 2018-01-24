// Omitting filedes.h to avoid circular define references
#include "communication.h"
#include "rt_stats.h"

#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#include "logging.h"

int proc_fd_limit(double *val) {
    struct rlimit rlim;
    int rtn = getrlimit(RLIMIT_NOFILE, &rlim);
    if (rtn < 0) {
        log_error("Error getting rlimit for fds");
        return -1;
    }
    *val = (double)rlim.rlim_cur;
    return 0;
}

