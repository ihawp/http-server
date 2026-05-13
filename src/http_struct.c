

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "helpers.h"
#include "hash_table.h"
#include "http_struct.h"

HTTPRequest *nhreq() {
	HTTPRequest *htr = xmalloc(sizeof(HTTPRequest));

	htr->method = xmalloc(REQ_METHOD_SIZE);

	if (htr->method == NULL) {
		free(htr);
		return NULL;
	}

	htr->path = xmalloc(REQ_PATH_SIZE);

	if (htr->path == NULL) {
		free(htr->method);
		free(htr);
		return NULL;
	}

	htr->http_version = xmalloc(REQ_HTTP_VERSION_SIZE);

	if (htr->http_version == NULL) {
		free(htr->method);
		free(htr->path);
		free(htr);
		return NULL;
	}

	return htr;
}

HTTPResponse *nhres() {
	HTTPResponse *htr = xmalloc(sizeof(HTTPResponse));
	
	if (htr == NULL) {
		return NULL;
	}

	return htr;
}

void free_http_response(
	HTTPResponse *htr
) {
	htr->status = 0;

    free(htr);
}

void free_http_request(
	HTTPRequest *hrq
) {
	// can be null when requests do not go past recv_header_chunks
	if (hrq->headers != NULL) {
		ht_destroy(hrq->headers);
	}

	if (hrq->body != NULL) {
		memset(hrq->body, 0, hrq->content_length);
		free(hrq->body);
	}

	free(hrq->header_storage);
	hrq->content_length = 0;

	memset(hrq->method, 0, REQ_METHOD_SIZE);
	memset(hrq->path, 0, REQ_PATH_SIZE);
	memset(hrq->http_version, 0, REQ_HTTP_VERSION_SIZE);

    free(hrq->method);
    free(hrq->path);
    free(hrq->http_version);

    free(hrq);
}