#include <sys/socket.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/syscall.h>

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
	pthread_t http_workers[MAX_WORKERS], accept_workers, delete_workers;
	struct epoll_event ev, events[MAX_EVENTS] = {0};
	struct process_data data = {0}; // holds hash table for user state
	
	signal(SIGINT, handle_sigint);
	printf("\e[1;1H\e[2J");

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
	data.pid = getpid();
	sfd = tcp_server(port);

	if (sfd == -1) {
		exit(EXIT_FAILURE);
	}

	if (listen(sfd, SOMAXCONN) == -1) {
		printfid("Failed to listen", data.pid);
		exit(EXIT_FAILURE);
	}

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
		pthread_create(&http_workers[i], NULL, (void*) http_worker, &data);
	}

	pthread_create(&accept_workers, NULL, (void*) accept_worker, &data);
	pthread_create(&delete_workers, NULL, (void*) delete_worker, &data);

	printfid("Server listening on port %s", data.pid, port);

	// accept user input to control the server!?

	pause();
}