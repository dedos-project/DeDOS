/**
 * @file socket_monitor.h
 *
 * Monitors an incoming port for messages from runtime or controller
 */

#ifndef SOCKET_MONITOR_H_
#define SOCKET_MONITOR_H_

/**
 * Adds the global controller to be monitored by the socket monitor
 * @param fd the file decriptor of the global controller
 * @return 0 on success, -1 on error
 */
int monitor_controller_socket(int fd);

/**
 * Adds a runtime to be monitored by the socket monitor
 * @param fd the file descriptor of the new runtime
 * @return 0 on success, -1 on error
 */
int monitor_runtime_socket(int fd);

/**
 * Starts (blocking) the socket monitor, listening on the provided port.
 * Also connects to the global controller at the provided address
 */
int run_socket_monitor(int local_port, struct sockaddr_in *ctrl_addr);

#endif
