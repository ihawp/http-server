#pragma once

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "line_in_memory_array.h"
#include "string_view.h"
#include "hash_table.h"
#include "http_struct.h"
#include "state_machine.h"

#define CHUNK_SIZE 512
#define BUFFER_CHUNK_SIZE 512 // must be at least 3
#define JSON_BUF_SIZE 512

// TODO: rename and organize and refactor and rethink!
#define MAX_EVENTS 10
#define MAX_WORKERS 3
#define HEADER_SIZE 256
#define MAX_CONTENT_LENGTH 8000
#define CLIENT_BUF_SIZE 1024
#define CHAR_SIZE sizeof(char)

typedef struct {
    char *body_start;
    int status; // 0 = ok, 1 = EAGAIN, -1 = error
} RecvHeaderResult;

typedef enum {
	RETRY_ERROR = 1
} Errors;

void free_http_request(
	HTTPRequest *hrq
);

void free_http_response(
	HTTPResponse *htr
);

char *file_to_content_type(
	char *path
);

const char *http_status_str(
	int code
);

void send_json_response(
	int *client_fd,
	int status,
	char *error_message	
);

FILE *open_file_from_path(
	char *path
);

int hex_digit(
	char c
);

int decode_url(
	char *str
);

int sanitize_path(
	char *path
);

int send_stream_file(
	int *client_fd,
	HTTPRequest *http_request,
	HTTPResponse *http_response,
	FILE *f
);

int extract_path_method_version(
	HTTPRequest *req,
	char *header,
	int count
);

int find_headers(
	HTTPRequest *http_request
);

RecvHeaderResult recv_header_chunks(
    int *client_fd,
    char *buffer,
    ssize_t *recv_count
);

int recv_header(
	char **body_start,
	char *headers,
	int client_fd,
	size_t *bs_size,
	ssize_t *recv_count,
	size_t *body_length
);

// TODO: post requests get stuck
int recv_body_chunks(
	int *client_fd,
	char *buffer,
	size_t content_length,
	size_t *body_length
);

int recv_body(
	int *client_fd,
	pid_t *tid,
	HTTPRequest *http_request,
	size_t *body_length
);

int move_body(
	int *client_fd,
	pid_t *tid,
	HTTPRequest *http_request,
	char *body_start,
	size_t *body_length
);

int handle_get_request(
	int *client_fd,
	pid_t *tid,
	HTTPRequest *http_request,
	HTTPResponse *http_response
);

int handle_request(
	int client_fd,
	pid_t tid,
	UserState *user_state
);

void *http_worker(
	void *data
);