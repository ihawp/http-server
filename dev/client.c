#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define HOST     "127.0.0.1"
#define PORT     "3000"
#define INTERVAL 10

static int send_all(int fd, const char *buf, size_t len) {
    while (len > 0) {
        ssize_t n = send(fd, buf, len, 0);
        if (n < 0) return -1;
        buf += n;
        len -= n;
    }
    return 0;
}

/* Read until we see the end of the HTTP response headers (\r\n\r\n) */
static int read_connect_response(int fd) {
    char buf[4096];
    size_t total = 0;

    while (total < sizeof(buf) - 1) {
        ssize_t n = recv(fd, buf + total, 1, 0);
        if (n <= 0) return -1;
        total++;
        if (total >= 4 &&
            buf[total-4] == '\r' && buf[total-3] == '\n' &&
            buf[total-2] == '\r' && buf[total-1] == '\n') {
            buf[total] = '\0';
            printf("Server response:\n%s\n", buf);
            /* Expect "HTTP/1.x 200 ..." */
            return (strncmp(buf, "HTTP/1.", 7) == 0 && buf[9] == '2') ? 0 : -1;
        }
    }
    return -1;
}

int main(void) {
    struct addrinfo hints, *res;
    int fd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(HOST, PORT, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return 1;
    }

    fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        perror("socket");
        freeaddrinfo(res);
        return 1;
    }

    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        freeaddrinfo(res);
        close(fd);
        return 1;
    }
    freeaddrinfo(res);
    printf("TCP connected to %s:%s\n", HOST, PORT);

    /* Send HTTP CONNECT handshake */
    const char *handshake =
        "CONNECT " HOST ":" PORT " HTTP/1.1\r\n"
        "Host: " HOST "\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";

    printf("Sending CONNECT handshake...\n");
    if (send_all(fd, handshake, strlen(handshake)) < 0) {
        perror("send handshake");
        close(fd);
        return 1;
    }

    /* Wait for 200 Connection Established */
    if (read_connect_response(fd) < 0) {
        fprintf(stderr, "CONNECT handshake failed\n");
        close(fd);
        return 1;
    }
    printf("Tunnel established — sending data every %d seconds\n", INTERVAL);

    /* Tunnel is open — send raw data in a loop */
    int seq = 0;
    while (1) {
        char buf[256];
        time_t now = time(NULL);
        int len = snprintf(buf, sizeof buf,
            "{\"seq\":%d,\"ts\":%ld,\"msg\":\"heartbeat\"}\n",
            ++seq, (long)now);

        if (send_all(fd, buf, len) < 0) {
            perror("send data");
            break;
        }
        printf("Sent: %s", buf);
        int recv_count = recv(fd, buf, 256, 0);

        printf("RECV_COUNT: %d\n", recv_count);

        sleep(INTERVAL);
    }

    close(fd);
    return 0;
}