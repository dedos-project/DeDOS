#include "communication.h"
#include "runtime.h"
#include "modules/webserver_msu.h"
#include "routing.h"
#include "dedos_msu_msg_type.h"
#include "dedos_msu_list.h"
#include "dedos_thread_queue.h" //for enqueuing outgoing control messages
#include "control_protocol.h"
#include "logging.h"
#include "modules/ssl_msu.h"
#include "modules/regex_msu.h"
#include "generic_msu.h"

void GetLine(char *Request, int Offset, char EndChar, char *out) {
    if (Request == NULL) {
        return;
    }

    int i;
    for (i = Offset; i <= strlen(Request); i++) {
        if (Request[i] == '\0' || Request[i] == EndChar) {
            //ToReturn = (char*)msu_data_alloc(self, i - Offset + 1);
            memcpy(out, &Request[Offset], i - Offset);
            break;
        }
    }
    out[i - Offset] = '\0';
    //return i - Offset;
}

int query_db(char *ip, int port, const char *query, int param, struct generic_msu *self)
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
    if (connect(sockfd, (struct sockaddr *)&db_addr, sizeof(db_addr)) < 0) {
        log_error("%s", "connect failed");
        close(sockfd);
        return -1;
    }

    // send request
    int req_buf_len= strlen(query) + 1 + how_many_digits(param);
    char *req_buf = malloc(req_buf_len + 1);
    snprintf(req_buf, req_buf_len + 1, "%s %d", query, param);
    if (send(sockfd, req_buf, req_buf_len, 0) == -1) {
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
        (void*) &db_addr, sizeof(struct sockaddr_in));
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

    char *memory = (char *) self->internal_state;
    if (memory != NULL) {
        int increment = (1<<12);
        int iter_size = memSize / 10;
        for (int i=0; i < iter_size; i+= increment){
            memory[i]++;
        }
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

/** Serializes the data of an msu_queue_item and sends it
 * to be deserialized by a remote msu.
 *
 * Frees the message after enqueuing
 *
 * @param src MSU sending the message
 * @param data item to be enqueued onto next MSU
 * @param dst MSU destination to receive the message
 * @return -1 on error, >=0 on success
 */
int webserver_send_remote(struct generic_msu *src, struct generic_msu_queue_item *data,
                        struct msu_endpoint *dst){
    struct dedos_intermsu_message *msg = malloc(sizeof(*msg));
    if (!msg){
        log_error("Unable to allocate memory for intermsu msg%s", "");
        return -1;
    }

    msg->dst_msu_id = dst->id;
    msg->src_msu_id = src->id;
    msg->data_id = data->id;

    // TODO: Is this next line right? src->proto_number?
    msg->proto_msg_type = src->type->proto_number;
    msg->payload_len = data->buffer_len;
    msg->payload = malloc(msg->payload_len);
    if (!msg->payload){
        log_error("Unable to allocate memory for intermsu payload %s", "");
        free(msg);
        return -1;
    }

    if (dst->type_id != DEDOS_REGEX_MSU_ID && dst->type_id != DEDOS_REGEX_ROUTING_MSU_ID) {
        memcpy(msg->payload, data->buffer, msg->payload_len);
    } else {
        struct regex_data_payload *regex_data =
                (struct regex_data_payload *)(data->buffer);
        struct ssl_data_payload *recv_data =
                (struct ssl_data_payload *)(regex_data->dst_packet);

        memcpy(msg->payload, regex_data, sizeof(*regex_data));
        memcpy(msg->payload + sizeof(*regex_data), recv_data, sizeof(*recv_data));
    }


    struct dedos_thread_msg *thread_msg = malloc(sizeof(*thread_msg));
    if (!thread_msg){
        log_error("Unable to allocate dedos_thread_msg%s", "");
        free(msg->payload);
        free(msg);
        return -1;
    }
    thread_msg->next = NULL;
    thread_msg->action = FORWARD_DATA;
    thread_msg->action_data = dst->ipv4;

    thread_msg->buffer_len = sizeof(*msg) + msg->payload_len;
    thread_msg->data = msg;

    /* add to allthreads[0] queue,since main thread is always at index 0 */
    /* need to create thread_msg struct with action = forward */

    int rtn = dedos_thread_enqueue(main_thread, thread_msg);
    if (rtn < 0){
        log_error("Failed to enqueue data in main thread queue%s", "");
        free(thread_msg);
        free(msg->payload);
        free(msg);
        return -1;
    }
    log_debug("Successfully enqueued msg in main queue, size: %d", rtn);
    return rtn;
}


#define MAX_LINE_LENGTH 128

/**
 * Called when the webserver MSU receives data from another MSU
 * @param self the Webserver MSU itself
 * @param input_data Data received by the MSU
 * @return type ID of next MSU to receive data on success, -1 on error
 */
int webserver_receive(struct generic_msu *self, struct generic_msu_queue_item *input_data) {
    if (self && input_data) {
        // printf("web_server_data_handler :Webserver MSU started processing\n");
        struct ssl_data_payload *recv_data = (struct ssl_data_payload *) (input_data->buffer);
        char *Request =  recv_data->msg;
        debug("DEBUG: WS MSU received data from source ssl msu %d",
              recv_data->sslMsuId);
        log_debug("Received request: %s", Request);

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
                    if (query_db(db_ip, db_port, "REQUEST", rand() % db_max_load, self) < 0) {
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
                    // += because the original buffer is still contained in the new buffer
                    input_data->buffer_len += sizeof(*regex_data);
                    return DEDOS_REGEX_ROUTING_MSU_ID;
                }
                else {
                    char ReturnOk[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length:";
                    char htmlDoc[] =
                        "<!DOCTYPE html>\n<html>\n<body>\n<h1>Dedos New Runtime MSU %03d</h1>\n</body>\n</html>";
                    char htmlDocOut[strlen(htmlDoc)];
                    sprintf(htmlDocOut,htmlDoc, self->id);

                    // Generate the document to send back to the client
                    int http_response_len = strlen(ReturnOk) +  strlen(htmlDocOut) + 20;
                    char *finalSend = (char *) malloc (http_response_len);
                    memset(finalSend, '\0', http_response_len);

                    snprintf(finalSend, http_response_len, "%s %d\r\n\r\n%s",
                             ReturnOk, (int) strlen(htmlDocOut), htmlDocOut);
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

int webserver_init(struct generic_msu *self, struct create_msu_thread_data *initial_state){
    self->internal_state = malloc(VIDEO_MEMORY / 10);
    return 0;
}

int webserver_destroy(struct generic_msu *self){
    if (self->internal_state != NULL){
        free(self->internal_state);
    }

    return 0;
}

/**
 * All instances of Webserver MSUs share this type information
 */
const struct msu_type WEBSERVER_MSU_TYPE = {
    .name="WebserverMSU",
    .layer=DEDOS_LAYER_APPLICATION,
    .type_id=DEDOS_WEBSERVER_MSU_ID,
    .proto_number=0,
    .init=NULL,
    .destroy=NULL,
    .receive=webserver_receive,
    .receive_ctrl=NULL,
    .route=default_routing,
    .deserialize=default_deserialize,
    .send_local=default_send_local,
    .send_remote=webserver_send_remote
};


