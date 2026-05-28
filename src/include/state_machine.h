#pragma once

#include <stdio.h>
#include <pthread.h>
#include "program_speed.h"
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
    struct program_speed speed;
    pthread_mutex_t mutex; // lock
} UserState;

typedef enum {
    HEADERS = 1,
    CHECK_HEADERS,
    CHECK_REQUEST_METHOD,
    GET,
    CONNECT,
    MOVE_BODY,
    BODY,
    RESPONSE,
    ERROR,
    FIN
} State;

UserState *nus(
    int client_fd
);

void free_user_state(
    UserState *user_state
);