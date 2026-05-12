#pragma once

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "hash_table.h"

typedef struct {
	ht *headers;
	char *header_storage;
	char *body;
	long content_length;

	// allocate on heap with malloc()
	char *method; // REQ_METHOD_SIZE
	char *path; // REQ_PATH_SIZE
	char *http_version; // REQ_HTTP_VERSION_SIZE
} HTTPRequest;

typedef struct {
	int status;
} HTTPResponse;