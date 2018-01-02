#ifndef GC_SOCKET_INTERFACE_THREAD__
#define GC_SOCKET_INTERFACE_THREAD__
#include <pthread.h>

int start_socket_interface_thread(pthread_t *thread, int port);

#endif
