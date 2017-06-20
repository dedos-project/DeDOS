#ifndef TMP_MSU_HEADERS_H_
#define TMP_MSU_HEADERS_H_

//Temporary declaration for controller
#define DEDOS_SOCKET_HANDLER_MSU_ID 600
#define DEDOS_SSL_READ_MSU_ID 500
#define DEDOS_WEBSERVER_MSU_ID 501

struct socket_handler_init_payload {
    int port;
    int domain; //AF_INET
    int type; //SOCK_STREAM
    int protocol; //0 most of the time. refer to `man socket`
    unsigned long bind_ip; //fill with inet_addr, inet_pton(x.y.z.y) or give IN_ADDRANY
    int target_msu_type;
};
#endif /* TMP_MSU_HEADERS_H_ */
