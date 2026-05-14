#include <sys/socket.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <signal.h>

#include "http.h"
#include "helpers.h"
#include "tcp_server.h"
#include "process_data.h"
#include "state_machine.h"
#include "graceful_shutdown.h"

int main(
	int argc,
	char **argv
) {
	char *port;
	int sfd, epc, result, client_fd, i, n, ectl;
	pthread_t workers[MAX_WORKERS];
	pthread_t *wp = workers;
	struct epoll_event ev, events[MAX_EVENTS] = {0};
	struct process_data data = {0}; // holds hash table for user state
	struct timespec ts;

	signal(SIGINT, handle_sigint);

	if (argc < 2) {
		printf(
			"Server startup failed, try:\n\n"
			"<Server Name> <PORT>\n\n"
			"Example:\n\n"
			"./server 2222\n"
		);
		exit(EXIT_FAILURE);
	}

	port = argv[1];
	sfd = tcp_server(port);

	if (sfd == -1) {
		exit(EXIT_FAILURE);
	}

	data.pid = getpid();

	if (listen(sfd, SOMAXCONN) == -1) {
		printfid("Failed to listen", data.pid);
		exit(EXIT_FAILURE);
	}

	printf("\e[1;1H\e[2J");
	printfid("Server listening on port %s", data.pid, port);
	
	epc = epoll_create1(0);
	ev.events = EPOLLIN | EPOLLEXCLUSIVE;
	ev.data.fd = sfd;
	if (epoll_ctl(epc, EPOLL_CTL_ADD, sfd, &ev) == -1) {
		printfid("sfd epoll_ctl", data.pid);
		exit(EXIT_FAILURE);
	}

	data.epc = epc;
	data.sfd = sfd;
	data.user_states = ht_create();

	pthread_mutex_init(&data.lock, NULL);
    pthread_cond_init(&data.ready, NULL);

	for (i = 0; i < MAX_WORKERS; i++) {
		pthread_create(&workers[i], NULL, (void*) http_worker, &data);
	}

	// remove expired clients (passed/at deadline)
	for (;;) {

		pthread_mutex_lock(&data.lock);

		if (ht_length(data.user_states) == 0) {
			continue;
		}

		// int expired_fds[ht_length(data.user_states) || 1] // (and remove above `if`)
		int expired_fds[ht_length(data.user_states)];
		int expired_count = 0;
		hti it = ht_iterator(data.user_states);
		
		while (ht_next(&it)) {
			UserState *us = it.value;
			clock_gettime(CLOCK_MONOTONIC, &ts);
			int64_t now = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

			bool is_expired = now >= us->deadline;
			bool over_retry = us->retries >= 3;

			if (is_expired || over_retry) {
				expired_fds[expired_count++] = us->client_fd;
			}
		}

		pthread_mutex_unlock(&data.lock);

		for (int i = 0; i < expired_count; i++) {
			pthread_mutex_lock(&data.lock);
			UserState *us = ht_get(data.user_states, HT_INT(expired_fds[i]));
			ht_remove(data.user_states, HT_INT(expired_fds[i]));
			pthread_mutex_unlock(&data.lock);

			if (us) {
				epoll_ctl(data.epc, EPOLL_CTL_DEL, us->client_fd, NULL);
				us->http_response->status = 408;
				send_json_response(
					&us->client_fd,
					us->http_response->status,
					"{"
						"\"error\": \"Request timed out\","
						"\"success\": false"
					"}"
				);
				close(us->client_fd);
				free_user_state(us);
			}
		}

		nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 50000000}, NULL); // 50ms
	}
}