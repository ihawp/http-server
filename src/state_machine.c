#include <stdio.h>
#include "state_machine.h"
#include "http_struct.h"
#include "helpers.h"

UserState *nus(int client_fd) {
    struct timespec ts;

    UserState *us = xmalloc(sizeof(UserState));
    if (us == NULL) return NULL;

    us->http_request = nhreq();
    if (us->http_request == NULL) { free(us); return NULL; }

    us->http_response = nhres();
    if (us->http_response == NULL) { free(us->http_request); free(us); return NULL; }

    us->client_fd = client_fd;
    us->state = HEADERS;
    us->retries = 0;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    us->deadline = ts.tv_sec * 1000 + ts.tv_nsec / 1000000 + 5000;

    pthread_mutex_init(&us->mutex, NULL);
    return us;
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