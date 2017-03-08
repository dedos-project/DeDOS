#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hash_function.h"

void print_hash(unsigned char* hash){
    int i;
    char buf[SHA_DIGEST_LENGTH*2];
    memset(buf, 0x0, SHA_DIGEST_LENGTH*2);
    for (i=0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf((char*)&(buf[i*2]), "%02x", hash[i]);
    }
    log_debug("Computed hash: %s",buf);
}

unsigned int compute_hash(int id){

    unsigned int num_digits;
    int num;
    unsigned char *hash;
    unsigned int uint_hash;
    char *data;

    num = id;
    num_digits = 0;
    uint_hash = 0;

    while(num){
        num_digits++;
        num = num/10;
    }
    // log_debug("Number of digits in %d: %d",id, num_digits);
    num_digits++; //for the \0

    data = (char*)malloc(sizeof(char) * num_digits);
    if(!data){
        log_error("Failed to malloc data %s","");
        return 0;
    }
    snprintf(data, num_digits, "%d", id);
    // log_debug("input id for hashing: %s",data);

    hash = (unsigned char*)malloc(sizeof(unsigned char) * SHA_DIGEST_LENGTH);
    if(!hash){
        log_error("Failed to malloc hash %s","");
        return 0;
    }
    size_t length = sizeof(char) * num_digits;

    int i = 0;
    memset(hash, 0x0, SHA_DIGEST_LENGTH);

    SHA1((unsigned char *)data, num_digits-1, hash);
    // print_hash(hash);

    free(data);
    for (i = SHA_DIGEST_LENGTH-1; i > SHA_DIGEST_LENGTH - 5; i--) {
        uint_hash = (uint_hash << 8) | hash[i];
    }

    free(hash);
    return uint_hash;
}
