#include <stdio.h>
#include "state_machine.h"
#include "http_struct.h"
#include "helpers.h"

UserState *nus(
    int client_fd
) {
    struct timespec ts;
    UserState *user_state = xmalloc(sizeof(UserState));

    if (user_state == NULL) {
        return NULL;
    }

    user_state->retries = 0;
    user_state->client_fd = client_fd;
    
    user_state->http_request = xmalloc(sizeof(HTTPRequest));
    memset(user_state->http_request, 0, sizeof(HTTPRequest));
    user_state->state = HEADERS;
    
    if (user_state->http_request == NULL) {
        free(user_state);
        return NULL;
    }

    user_state->http_response = xmalloc(sizeof(HTTPResponse));
    memset(user_state->http_response, 0, sizeof(HTTPResponse));
    if (user_state->http_response == NULL) {
        free(user_state);
        free_http_request(user_state->http_request);
        return NULL;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts);
    user_state->deadline = ts.tv_sec * 1000 + ts.tv_nsec / 1000000 + 5000; // now + [X]ms

    pthread_mutex_init(&user_state->mutex, NULL);

    return user_state;
}

void free_user_state(
    UserState *user_state
) {
    user_state->retries = 0;
    user_state->client_fd = 0;
    user_state->state = 0;
    user_state->deadline = time(NULL);

    if (user_state->http_request != NULL) {
        free_http_request(user_state->http_request);
    }

    if (user_state->http_response != NULL) {
        free_http_response(user_state->http_response);            
    }

    pthread_mutex_destroy(&user_state->mutex);

    free(user_state);
}