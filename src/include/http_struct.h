#pragma once

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "hash_table.h"

#define REQ_METHOD_SIZE 16
#define REQ_PATH_SIZE 256
#define REQ_HTTP_VERSION_SIZE 24

typedef struct {
	ht *headers;
	char *header_storage;
	char *body;
	char *body_start;
	char *method; // REQ_METHOD_SIZE
	char *path; // REQ_PATH_SIZE
	char *http_version; // REQ_HTTP_VERSION_SIZE
	
	long content_length;
	ssize_t recv_count;
	size_t bs_size; 
	size_t body_length;
} HTTPRequest;

typedef struct {
	int status;
} HTTPResponse;

void free_http_response(
	HTTPResponse *htr
);

void free_http_request(
	HTTPRequest *hrq
);

HTTPRequest *nhreq();
HTTPResponse *nhres();