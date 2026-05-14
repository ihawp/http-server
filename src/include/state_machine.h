#pragma once

#include <stdio.h>
#include <pthread.h>
#include "http_struct.h"

#define MAX_EPOLL_RETRIES 3

// forward declartion...not working

typedef struct {
    int retries;
    int client_fd;
    int state; // might not need state if we are only using for the one case
    int64_t deadline;
    HTTPRequest *http_request;
    HTTPResponse *http_response;
    pthread_mutex_t mutex; // lock
} UserState;

typedef enum {
    HEADERS = 1,
    MOVE_BODY,
    BODY,
    RESPONSE,
    GET,
    ERROR,
    FIN
} State;

UserState *nus(
    int client_fd
);

void free_user_state(
    UserState *user_state
);