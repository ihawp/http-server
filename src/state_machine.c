#include <stdio.h>
#include "state_machine.h"
#include "http.h"
#include "http_struct.h"
#include "helpers.h"

UserState *nus(
    int client_fd
) {
    UserState *user_state = xmalloc(sizeof(UserState));

    user_state->retries = 0;
    user_state->client_fd = client_fd;
    user_state->deadline = time(NULL) + 0.05; // time() returns seconds, have 50 ms deadline
    user_state->lock = 0;
    user_state->http_request = xmalloc(sizeof(HTTPRequest));
    user_state->state = HEADERS;
    
    if (user_state->http_request == NULL) {
        return NULL;
    }

    user_state->http_response = xmalloc(sizeof(HTTPResponse));
    if (user_state->http_request == NULL) {
        free_http_request(user_state->http_request);
        return NULL;
    }

    pthread_mutex_init(&user_state->mutex, NULL);

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

    pthread_mutex_destroy(&user_state->mutex);

    return 0;
}