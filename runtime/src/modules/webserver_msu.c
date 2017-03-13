#include "runtime.h"
#include "modules/webserver_msu.h"
#include "communication.h"
#include "routing.h"
#include "dedos_msu_msg_type.h"
#include "dedos_msu_list.h"
#include "dedos_thread_queue.h" //for enqueuing outgoing control messages
#include "control_protocol.h"
#include "logging.h"
#include "modules/ssl_msu.h"
#include "modules/regex_msu.h"
#include "generic_msu.h"

int GetLine(char *Request, int Offset, char EndChar, char *out) {
    if (Request == NULL) {
        return  NULL;
    }

    int Start = Offset;
    int End = Offset;
    int i;
    for (i = Offset; i <= strlen(Request); i++) {
        if (Request[i] == '\0' || Request[i] == EndChar) {
            //ToReturn = (char*)msu_data_alloc(self, i - Offset + 1);
            memcpy(out, &Request[Offset], i - Offset);
            break;
        }
    }
    out[i - Offset] = '\0';
    return i - Offset;;
}

int query_db(char *ip, int port, const char *query, int param)
{
    int sockfd, optval = 1;
    struct sockaddr_in db_addr;

    // create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        log_error("%s", " failure opening socket");
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
        log_error("%s", " failed to set SO_REUSEPORT");
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        log_error("%s", " failed to set SO_REUSEADDR");
    }

    // build db server's internet address
    bzero((char *)&db_addr, sizeof(db_addr));
    db_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &(db_addr.sin_addr));
    db_addr.sin_port = htons(port);

    // create a connection
    if (connect(sockfd, (struct sockaddr_in*)&db_addr, sizeof(db_addr)) < 0) {
        log_error("%s", "connect failed");
        close(sockfd);
        return -1;
    }

    // send request
    int req_buf_len= 15;
    char *req_buf = (char *) malloc(req_buf_len);
    sprintf(req_buf, "%s %d\0", query, param);
    if (send(sockfd, req_buf, strlen(req_buf), 0) == -1) {
        log_error("%s", "send failed");
        free(req_buf);
        return -1;
    }

    // receive response, expecting OK\n
    int res_buf_len = 3;
    int memSize;
    char res_buf[res_buf_len];
    memset((char *) &res_buf, 0, sizeof(res_buf));
    size_t res_len = 0;

    res_len = recvfrom(sockfd, res_buf, res_buf_len, 0,
        (void*) &db_addr, sizeof(db_addr));
    if (res_len < 0) {
        log_error("%s", "recv failed");
        return -1;
    } else {
        if (strncmp(res_buf, "00\n", 3) == 0) {
            memSize = (VIDEO_MEMORY);
        } else if (strncmp(res_buf, "01\n", 3) == 0) {
             memSize = (AUDIO_MEMORY);
        } else if (strncmp(res_buf, "02\n", 3) == 0) {
            memSize = (IMAGE_MEMORY);
        } else if (strncmp(res_buf, "03\n", 3) == 0) {
            memSize = (TEXT_MEMORY);
        } else {
            log_error("%s", "incorrect reponse from db server");
            free(req_buf);
            close(sockfd);
            return -1;
        }
    }

    char *memory = (char *) malloc (memSize / 10);
    if (memory != NULL) {
        memset(memory, 0, memSize / 10);
    }

    free(memory);
    free(req_buf);
    close(sockfd);

    return 0;
}

#define MAX_REGEX_VALUE_LENGTH 128
#define REGEX_KEY "regex="
/**
 * Finds the first occurance of "regex=" in the string referenced by *request,
 * and stores the value in the string *val_out
 *
 * @param request URL that is being parsed
 * @param val_out Output variable, filled with the value of "regex" from the URL
 */
void requested_regex_value(char *request, char *val_out){
    const int keylen = strlen(REGEX_KEY);
    char *regex_portion = strstr(request, REGEX_KEY);
    if (regex_portion == NULL){
        val_out[0] = '\0';
        return;
    }
    char *value = &regex_portion[keylen];
    int i;
    for (i=0; value[i]!='?' &&
              value[i]!='&' &&
              value[i]!=NULL &&
              value[i]!=' ' && i < MAX_REGEX_VALUE_LEN; i++);
    strncpy(val_out, value, i);
    val_out[i]='\0';
}

#define MAX_LINE_LENGTH 128

/**
 * Called when the webserver MSU receives data from another MSU
 * @param self the Webserver MSU itself
 * @param input_data Data received by the MSU
 * @return type ID of next MSU to receive data on success, -1 on error
 */
int webserver_receive(local_msu *self, msu_queue_item *input_data) {
    if (self && input_data) {
        // printf("web_server_data_handler :Webserver MSU started processing\n");
        int ret;
        struct ssl_data_payload *recv_data = (struct ssl_data_payload *) (input_data->buffer);
        char *Request =  recv_data->msg;
        debug("DEBUG: WS MSU received data from source ssl msu %d",
              recv_data->sslMsuId);

        if (Request != NULL) {
            char FirstLine[MAX_LINE_LENGTH];
            char RequestType[MAX_LINE_LENGTH];
            char RequestPage[MAX_LINE_LENGTH];
            GetLine(Request, 0, '\n', FirstLine);
            GetLine(FirstLine, 0, ' ', RequestType);
            GetLine(FirstLine, strlen(RequestType) + 1, ' ', RequestPage);

            log_debug("First line of request: %s", FirstLine);

            if (strcmp(RequestType, "GET") == 0) {
                log_debug("A GET Request:\n%s", Request);

                if (strstr(RequestPage, "database") != NULL) {
                    if (query_db(db_ip, db_port, "REQUEST", rand() % db_max_load) < 0) {
                        log_error("%s", "error querying database");
                    }
                }

                if (strstr(RequestPage, REGEX_KEY) != NULL ) {
                    struct regex_data_payload *regex_data = malloc(sizeof(*regex_data));

                    char lookup[MAX_REGEX_VALUE_LEN];
                    requested_regex_value(RequestPage, lookup);

                    regex_data->dst_type = DEDOS_SSL_WRITE_MSU_ID;
                    regex_data->dst_packet = input_data->buffer;
                    strncpy(regex_data->string, lookup, strlen(lookup) + 1);
                    input_data->buffer = regex_data;
                    input_data->buffer_len = sizeof(*regex_data);
                    return DEDOS_REGEX_MSU_ID;
                }
                else {
                    char ReturnOk[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length:";
                    char htmlDoc[] =
                        "<!DOCTYPE html>\n<html>\n<body>\n<h1>Dedos New Runtime</h1>\n</body>\n</html>";

                    // Generate the document to send back to the client
                    int http_response_len = strlen(ReturnOk) +  strlen(htmlDoc) + 20;
                    char *finalSend = (char *) malloc (http_response_len);
                    memset(finalSend, '\0', http_response_len);

                    snprintf(finalSend, http_response_len, "%s %d\r\n\r\n%s",
                             ReturnOk, (int) strlen(htmlDoc), htmlDoc);
                    strncpy(recv_data->msg, finalSend, strlen(finalSend) + 1);
                    recv_data->type = WRITE;

                    free(finalSend);

                    log_debug("Retuning rcv_data->msg=%s", recv_data->msg);

                    return DEDOS_SSL_WRITE_MSU_ID;
                }
            } else {
                log_warn("Webserver received unknown request type %s", RequestType);
            }
        } else {
            log_warn("Webserver received NULL request type", "");
        }
    } else {
        log_warn("Either self or input data to webserver was null??","");
    }
    return -1;
}

/**
 * All instances of Webserver MSUs share this type information
 */
const msu_type WEBSERVER_MSU_TYPE = {
    .name="WebserverMSU",
    .layer=DEDOS_LAYER_APPLICATION,
    .type_id=DEDOS_WEBSERVER_MSU_ID,
    .proto_number=0,
    .init=NULL,
    .destroy=NULL,
    .receive=webserver_receive,
    .receive_ctrl=NULL,
    .route=round_robin,
    .deserialize=default_deserialize,
    .send_local=default_send_local,
    .send_remote=default_send_remote
};


