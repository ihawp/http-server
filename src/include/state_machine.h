#pragma once

#include <stdio.h>
#include <time.h>
#include "http.h"

#define MAX_EPOLL_RETRIES 3

typedef struct {
    int retries;
    int client_fd;
    int lock; // client_fd is in use (request being processed)
    int state; // might not need state if we are only using for the one case
    time_t deadline;
    HTTPRequest *http_request;
    HTTPResponse *http_response;
} UserState;

typedef enum {
    HEADERS = 1,
    BODY,
    RESPONSE,
    FINISHED
} State;

UserState *nus(
    int client_fd,
) {
    UserState *user_state = xmalloc(sizeof(UserState));
    user_state->retries = 0;
    user_state->client_fd = client_fd;
    user_state->deadline = time(NULL) + 0.05; // time() returns seconds, have 50 ms deadline
    user_state->lock = 0;
    user_state->http_request = xmalloc(sizeof(HTTPRequest));
    if (user_state->http_request == NULL) return NULL;

    user_state->http_response = xmalloc(sizeof(HTTPResponse));
    if (user_state->http_request == NULL) {
        free_http_request(user_state->http_request);
        return NULL;
    }

    return user_state;
}

int free_user_state(
    UserState *user_state
) {
    user_state->retries = 0;
    user_state->lock = 0;
    user_state->client_fd = 0;
    user_state->state = 0;
    user_state->deadline = time(NULL);

    free_http_request(user_state->http_request);
    free_http_response(user_state->http_response);

    return 0;
}