#include <sys/socket.h>
#include <pthread.h>
#include <sys/epoll.h>

#include "http.h"
#include "helpers.h"
#include "tcp_server.h"
#include "process_data.h"
#include "state_machine.h"

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
	UserState *us;

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
	pthread_mutex_init(&data.lock, NULL);
    pthread_cond_init(&data.ready, NULL);

	for (i = 0; i < MAX_WORKERS; i++) {
		pthread_create(&workers[i], NULL, (void*) http_worker, &data);
	}

	data.user_states = ht_create();

	// remove expired clients (passed/at deadline)
	for (;;) {
		// collect expired, unlocked keys
		ht_key expired[50];
		size_t n = 0;

		hti it = ht_iterator(data.user_states);
		while (ht_next(&it)) {
			us = it.value;
			pthread_mutex_lock(&us->mutex);
			bool is_expired = !us->lock && time(NULL) >= us->deadline;
			pthread_mutex_unlock(&us->mutex);

			if (is_expired) {
				expired[n++] = it.key;
			}
		}

		// then remove them
		for (size_t i = 0; i < n; i++) {
			ht_remove(data.user_states, expired[i]);
			// remove from epoll waitlist too.
		}

		sleep(1);
	}
}