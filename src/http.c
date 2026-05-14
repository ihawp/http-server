#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <stdarg.h>

#include "http.h"
#include "line_in_memory_array.h"
#include "program_speed.h"
#include "helpers.h"
#include "tcp_server.h"
#include "process_data.h"
#include "hash_table.h"
#include "string_view.h"
#include "state_machine.h"
#include "http_struct.h"

char *file_to_content_type(
	char *path
) {
	const char *ext = strrchr(path, '.');

	if (!ext) return "application/octet-stream";
	if (strcmp(ext, ".html") == 0) return "text/html";
	if (strcmp(ext, ".css") == 0) return "text/css";
	if (strcmp(ext, ".js") == 0) return "application/javascript";
	if (strcmp(ext, ".json") == 0) return "application/json";
	if (strcmp(ext, ".png") == 0) return "image/png";
	if (strcmp(ext, ".jpg") == 0) return "image/jpeg";
	if (strcmp(ext, ".webp") == 0) return "image/webp";
	return "application/octet-stream";
}

const char *http_status_str(
	int code
) {
	switch (code) {
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 304: return "Not Modified";
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 408: return "Request Timeout";
		case 409: return "Conflict";
		case 413: return "Payload Too Large";
		case 415: return "Unsupported Media Type";
		case 422: return "Unprocessable Entity";
		case 429: return "Too Many Requests";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
		case 504: return "Gateway Timeout";
		default: return "Unknown";
	}
}

void send_json_response(
	int *client_fd,
	int status,
	char *error_message	
) {
	char message[RESPONSE_BUF_SIZE];
	int message_length;

	message_length = snprintf(
		message,
		sizeof(message),
		"HTTP/1.1 %d %s\r\n"
		"Content-Type: application/json\r\n"
		"Connection: close\r\n"
		"\r\n"
		"%s",
		status,
		http_status_str(status),
		error_message
	);

	send_wrapper(client_fd, message, message_length);
}

// Free LLM software generated this hex_digit function
int hex_digit(
	char c
) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Free LLM software generated this decode_url function
int decode_url(
	char *str
) {
    char *read = str;
    char *write = str;

    while (*read) {
        if (*read == '%') {
            int hi = hex_digit(read[1]);
            int lo = hex_digit(read[2]);
            if (hi < 0 || lo < 0) return -1; // malformed
            *write++ = (char)(hi << 4 | lo);
            read += 3;
        } else if (*read == '+') {
            *write++ = ' ';  // form encoding, optional
            read++;
        } else {
            *write++ = *read++;
        }
    }

    *write = '\0';
    return 0;
}

int sanitize_path(
	char *path
) {
	if (strstr(path, "/..") != NULL) return -1;
	if (strstr(path, " ") != NULL) return -2;
	if (strstr(path, "//") != NULL) return -3;
	if (path[0] != 0x2F) return -4; // probably not possible
	return 0;
}

// If you don't pass this to send_stream_file,
// make sure to close your file descriptor
FILE *open_file_from_path(
	char *path
) {
	char public_path[PATH_SIZE];
	FILE *f;

	printf("Path: %s\n", path);

	if (decode_url(path) < 0) {
		return NULL;
	}

	if (strcmp(path, "/") == 0) {
		strcpy(path, "/index.html");
	}

	if (sanitize_path(path) < 0) {
		return NULL;
	}

	// for safety you could split apart the path 
	// and rebuild it to a hidden internal structure
	// I will not do that yet

	snprintf(public_path, PATH_SIZE, "public/%s", path);
	f = fopen(public_path, "rb");

	if (f == NULL) {
		return NULL;
	}
	
	return f;
}

int send_stream_file(
	int *client_fd,
	HTTPRequest *http_request,
	HTTPResponse *http_response,
	FILE *f
) {
	char buffer[CHUNK_SIZE], response[CHUNK_SIZE], hex_header[16];
	int response_len, byte_count, hex_header_len;

	response_len = snprintf(
		response, 
		sizeof(response), 
		"HTTP/1.1 %d %s\r\n"
		"Content-Type: %s\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Connection: keep-alive\r\n"
		"\r\n",
		http_response->status,
		http_status_str(http_response->status),
		file_to_content_type(http_request->path)
	);

	// if send_wrapper fails we should exit with error in all cases
	if (send_wrapper(client_fd, response, response_len) < 0) {
		fclose(f);
		return -1;
	}

	for (;;) {
		// -2 for trailing \r\n
		byte_count = fread(buffer, CHAR_SIZE, CHUNK_SIZE - 2, f);
	
		if (byte_count) {
			memcpy(buffer + byte_count, "\r\n", 2);

			hex_header_len = snprintf(
				hex_header, 
				sizeof(hex_header), 
				"%x\r\n", 
				byte_count
			);

			if (send_wrapper(client_fd, hex_header, hex_header_len) < 0 ||
				send_wrapper(client_fd, buffer, byte_count + 2) < 0
			) {
				fclose(f);
				return -1;
			}
			
		}

		if (feof(f) != 0) break;
		if (ferror(f) != 0) { // could return -1 to indicate error and do seperate cleanup, but will just allow regular cleanup for now
			break;
		}
	}
	
	fclose(f);
	
	if (send_wrapper(client_fd, "0\r\n\r\n", 5) < 0) {
		return -1;
	}
	
	// don't call shutdown(...) if send_wrapper(...) fails
	// client connection is already closed
	shutdown(*client_fd, SHUT_WR);
	return 0;
}

int extract_path_method_version(
	HTTPRequest *req,
	char *header,
	int count
) {
	StringView svh = (StringView) {
		.string = header,
		.count = count
	};
	StringView fsvh = split_by_delim(&svh, 0x20);
	StringView path = split_by_delim(&svh, 0x20);

	if (path.count >= REQ_PATH_SIZE) {
		return -414; // return a 414
	}

	SV_to_memory(req->method, REQ_METHOD_SIZE, &fsvh);
	SV_to_memory(req->path, REQ_PATH_SIZE, &path);
	SV_to_memory(req->http_version, REQ_HTTP_VERSION_SIZE, &svh);

	return 0;
}

int find_headers(
	HTTPRequest *http_request
) {
	http_request->headers = ht_create();
	StringView s = sv(http_request->header_storage), key = {0}, value = {0};
	int last_line = 0, count;
	char *line_start;

	for (int i = 0; i < s.count; i++) {
		if (i + 1 >= s.count) {
			break;
		}
	
		// Checks for \r\n (carriage return, newline)
		if (s.string[i] == 0x0D && s.string[i + 1] == 0x0A) {

			line_start = (last_line == 0) ? s.string : &s.string[last_line + 2];
			count = (int)(s.string + i - line_start);

			if (last_line == 0) {
				if (extract_path_method_version(http_request, line_start, count) < 0) {
					break;
				}
			} else {
				value = (StringView) {
					.string = line_start, 
					.count = count
				};
				key = split_by_delim(&value, 0x3A);
				
				if (key.count == 0) continue;

				char keybuffer[key.count + 1], valuebuffer[value.count + 1];

				trim_by_delim(&key, 0x20);
				trim_by_delim(&value, 0x20);
				SV_to_memory(keybuffer, key.count + 1, &key);
				SV_to_memory(valuebuffer, value.count + 1, &value);

				if (strcmp(keybuffer, "Content-Length") == 0) {
					http_request->content_length = strtol(valuebuffer, NULL, 10);
				}

				ht_set(http_request->headers, HT_STR(keybuffer), valuebuffer);
			}

			last_line = i;
		}
	}

	return http_request->headers->length;
}

RecvHeaderResult recv_header_chunks(
    int *client_fd,
    char *buffer,
    ssize_t *recv_count
) {
    size_t max_header_size = CLIENT_BUF_SIZE;
    int status;
    char *mmp;

    for (;;) {
        status = recv_chunks(client_fd, buffer, recv_count, &max_header_size);

        if (status == RETRY_ERROR) {
			return (RecvHeaderResult){NULL, 1};
		}
        
		if (status == -1 || status == 2) {
			return (RecvHeaderResult){ NULL, -1 };
		}

        mmp = memmem(buffer, *recv_count, "\r\n\r\n", 4);
        if (mmp != NULL) {
            buffer[*recv_count] = '\0';
            return (RecvHeaderResult){ mmp, 0 };
        }
    }
}

int recv_header(
    char **body_start,
    char *headers,
    int client_fd,
    size_t *bs_size,
    ssize_t *recv_count,
    size_t *body_length
) {
    RecvHeaderResult result = recv_header_chunks(&client_fd, headers, recv_count);

    if (result.status == 1) return RETRY_ERROR;   // EAGAIN
    if (result.status == -1) return -1;  // error

    *body_start = result.body_start;
    *bs_size = *body_start - headers;
    *body_length = *recv_count - *bs_size - 4;
    **body_start = '\0';
    *body_start += 4;
    return 0;
}

int recv_body_chunks(
	int *client_fd,
	char *buffer,
	size_t content_length,
	size_t *body_length
) {
	int status;

	for (;;) {
		if (*body_length >= content_length) {
			break;
		}

		status = recv_chunks(
			client_fd,
			buffer,
			body_length, // is total count of bytes received for body
						 // is incremented inside recv_chunks
			&content_length
		);

		// EAGAIN or EWOULDBLOCK (RETRY_ERROR)
		if (status == RETRY_ERROR) {
			return RETRY_ERROR;
		}

		if (status == -1 || status == 2) {
			if (*body_length == 0) {
				return -1;
			}
			break;
		}
	}

	buffer[*body_length] = '\0';
	return 0;
}

int recv_body(
	int *client_fd,
	pid_t *tid,
	HTTPRequest *http_request,
	size_t *body_length
) {
	int rbc;

	rbc = recv_body_chunks(
		client_fd, 
		http_request->body, 
		(size_t) http_request->content_length, 
		body_length
	);

	if (rbc == RETRY_ERROR) {
		// EAGAIN or EWOULDBLOCK
		// send up chain
		return RETRY_ERROR;
	}

	if (rbc == -1) {
		printfid("Failed to recieve body chunks", *tid);
		return -1;
	}
	
	return 0;
}

int move_body(
	int *client_fd,
	pid_t *tid,
	HTTPRequest *http_request,
	char *body_start,
	size_t *body_length
) {
	if (http_request->content_length >= MAX_CONTENT_LENGTH - CLIENT_BUF_SIZE) {
		return -1; // TODO: can start making custom error codes #define OVER_LIMIT 10 for preset error responses (in json or etc)
	}

	if (*body_length == http_request->content_length) {
		printfid("Whole body found", *tid);
	}

	// stops program crash when content length is omitted, like:
	// "Content-Length: "
	// memmove causes crash since it will assume that the http_request->body
	// has enough memory to store all octets from body_start -> body_start + body_length
	if (http_request->content_length == 0 
		|| http_request->content_length <= *body_length) {
		return -1;
	}

	http_request->body = xmalloc(http_request->content_length + 1);
	if (http_request->body == NULL) {
		printfid("Failed to allocated memory for body", *tid);
		return -1;
	}

 	// move the originally (potentially) captured body content
	// into the proper body container: http_request->body
	memmove(http_request->body, body_start, *body_length);

	return 0;
}

int handle_get_request(
	int *client_fd,
	pid_t *tid,
	HTTPRequest *http_request,
	HTTPResponse *http_response
) {
	FILE *f = open_file_from_path(http_request->path);

	if (f == NULL) {
		printfid("Failed to open file", *tid);
		return -1;
	}

	if (send_stream_file(client_fd, http_request, http_response, f) == -1) {
		printfid("Failed to stream file", *tid);
		return -1;
	}

	return 0;
}

int handle_request(
	int client_fd,
	pid_t tid,
	UserState *user_state
) {
	int result;

	if (user_state->retries >= 3) {
		printfid("Too many retries for client: %d", tid, client_fd);
		return -1;
	}

	while (user_state->state != FIN) {
		switch (user_state->state) {
			case HEADERS:
				result = recv_header(
					&user_state->http_request->body_start,
					user_state->http_request->header_storage,
					client_fd,
					&user_state->http_request->bs_size,
					&user_state->http_request->recv_count,
					&user_state->http_request->body_length
				);

				if (result == RETRY_ERROR) {
					return RETRY_ERROR;
				}

				if (result < 0) {
					return -1;
				}

				if (find_headers(user_state->http_request) == 0) {
					return -1;
				}

				user_state->state = MOVE_BODY;

				if (strcmp(user_state->http_request->method, "GET") == 0) {
					user_state->state = GET;
				}

				break;
			case GET:
				if (handle_get_request(
					&client_fd, 
					&tid, 
					user_state->http_request, 
					user_state->http_response
				) < 0) {
					printfid("Failed to handle GET request", tid);
					return -1;
				}

				user_state->state = FIN;
				break;
			case MOVE_BODY:
				if (move_body(
					&client_fd, 
					&tid, 
					user_state->http_request, 
					user_state->http_request->body_start, 
					&user_state->http_request->body_length
				) < 0) {
					return -1;
				}

				user_state->state = BODY;
				break;
			case BODY:
				result = recv_body(
					&client_fd, 
					&tid, 
					user_state->http_request, 
					&user_state->http_request->body_length
				);

				if (result == RETRY_ERROR) {
					return RETRY_ERROR;
				}
				if (result < 0) {
					printfid("Failed to handle POST request", tid);
					return -1;
				}

				user_state->state = RESPONSE;
				break;
			case RESPONSE:
				// TODO: do something with body/request
				// send a response for now:
				send_json_response(
					&client_fd, 
					user_state->http_response->status, 
					"{"
						"\"success\": true,"
						"\"message\": \"We recieved your data!\""
					"}"
				);

				user_state->state = FIN;
				break;
			case ERROR:
				user_state->http_response->status = 501;
				return -1;
				break;
			default:
				printfid("Failed to find state", tid);
				break;
		}
	}

	return 0;
}

void *http_worker(
	void *data
) {
	struct process_data *wd = data;
	struct sockaddr_storage peer_addr = {0};
	struct epoll_event ev, events[MAX_EVENTS] = {0};
	socklen_t peer_addrlen = sizeof(struct sockaddr_storage);
	int client_fd, n, ectl, epoll_result, fd, hr_result;
	pid_t tid;
	struct program_speed speed = {0};

	ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
	tid = syscall(SYS_gettid);
	int *tid_p = &tid;

	printfid("Worker Started", tid);

	for (;;) {
		epoll_result = epoll_wait(wd->epc, events, MAX_EVENTS, -1);
		if (epoll_result < 0) continue;
		
		for (n = 0; n < epoll_result; ++n) {
			if (events[n].data.fd == wd->sfd) {

				client_fd = accept(
					wd->sfd, 
					(struct sockaddr*) &peer_addr, 
					(socklen_t*) &peer_addrlen
				);

				if (client_fd == -1) {
					printfid("client_fd", tid);
					continue;
				}

				if (setnonblocking(client_fd) == -1) {
					printfid("blocking", tid);
				}

				ev.data.fd = client_fd;
				ectl = epoll_ctl(wd->epc, EPOLL_CTL_ADD, client_fd, &ev);
				
				if (errno == EEXIST) {
					printfid("EEXIST", tid);
				}

				if (ectl == -1) {
					printfid("setnonblocking epoll_ctl", tid);
					exit(EXIT_FAILURE);
				}
			} else {
				// data ready
				UserState *us;
				fd = events[n].data.fd;

				// get information from hash table if available (state)
				us = ht_get(wd->user_states, HT_INT(fd));

				if (us == NULL) {
					us = nus(fd);
					if (us != NULL) {
						ht_set(wd->user_states, HT_INT(fd), us);  // save immediately
					}
				}

				// `us` could be NULL because nus could fail to xmalloc(...)
				if (us != NULL) {

					us->http_response->status = 200;
					ps_cap(&speed.start);
					
					hr_result = handle_request(fd, tid, us);

					if (hr_result == RETRY_ERROR) {
						us->retries++;
						ev.data.fd = fd;
						epoll_ctl(wd->epc, EPOLL_CTL_MOD, fd, &ev);
						continue;
					}

					if (hr_result != 0) {
						// TODO: remove line below after setting 
						// status where required throughout program
						us->http_response->status = 500;
						send_json_response(
							&fd, 
							us->http_response->status, 
							"{"
								"\"error\": \"Failed to handle request\","
								"\"success\": false"
							"}"
						);
						continue;
					}

					ps_cap(&speed.end);
					ps_print_elapsed(&speed, tid_p);
					free_user_state(us);
				}

				// move ht_remove inside above if?
				ht_remove(wd->user_states, HT_INT(fd));
				epoll_ctl(wd->epc, EPOLL_CTL_DEL, fd, NULL);
				close(fd);
			}
		}
	}

	printfid("Worker Exiting", tid);
}