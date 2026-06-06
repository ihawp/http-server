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
	char message[JSON_BUF_SIZE];
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
	char public_path[REQ_PATH_SIZE];
	FILE *f;

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

	snprintf(public_path, REQ_PATH_SIZE, "public/%s", path);
	f = fopen(public_path, "rb");

	if (f == NULL) {
		return NULL;
	}
	
	return f;
}

/*
Only called if the client is sending messages as chunks
using Transfer-Encoding: Chunked for their POST, etc request.

NOT included in http.h yet
*/
int recv_client_stream(
	int *client_fd,
	HTTPRequest *http_request,
	HTTPResponse *http_response
) {
	// use hex_digit(...);

	
}

int send_stream_file(
	int *client_fd,
	HTTPRequest *http_request,
	HTTPResponse *http_response,
	FILE *f
) {
	char buffer[BUFFER_CHUNK_SIZE], response[CHUNK_SIZE], hex_header[16];
	int response_len, byte_count, hex_header_len;

	// check if client accept chunked transfer encoding.

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
		byte_count = fread(buffer, CHAR_SIZE, BUFFER_CHUNK_SIZE - 2, f);
	
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

	if (fsvh.count >= REQ_METHOD_SIZE
		|| svh.count >= REQ_HTTP_VERSION_SIZE) {
		return -1;
	}

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

	for (int i = 0; i <= s.count; i++) {

		// Checks for \r\n (carriage return, newline) CRLF
		if (s.string[i] == 0x0D && s.string[i + 1] == 0x0A 
			|| i == s.count) {
				// i == s.count because (for final header):
				// I add a null terminator in recv_header(...) before the \r\n\r\n
				// so there is no \r\n\r\n in this StringView s after the final
				// header character

			line_start = (last_line == 0) ? s.string : &s.string[last_line + 2];
			count = (int)(s.string + i - line_start);

			if (last_line == 0) {
				if (extract_path_method_version(http_request, line_start, count) < 0) {
					break;
				}
			} else {
				value = (StringView) { .string = line_start, .count = count };
				key = split_by_delim(&value, 0x3A);
				if (key.count == 0) continue;

				// to save the value, key can be local since number
				// is used for actual indexing based on passed key
				char keybuffer[key.count + 1];
				char *valuebuffer = xmalloc(value.count + 1);
				if (valuebuffer == NULL) {
					return -1;
				}

				trim_by_delim(&key, 0x20);
				trim_by_delim(&value, 0x20);

				SV_to_memory(keybuffer, key.count + 1, &key);
				SV_to_memory(valuebuffer, value.count + 1, &value);

				memset(&key, 0, sizeof(StringView));
				memset(&key.string, 0, key.count);
				memset(&value, 0, sizeof(StringView));
				memset(&value.string, 0, value.count);

				// can remove this if from here and check later when using
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

/*
Returns:
- 1 client still sending/receiving info
- (-1) client/server failed
- 0 successfully finished talking to client

Implementing CONNECT is bad, because CONNECT is bad, but...
it is a part of HTTP/1.1!
*/
int handle_connect_request(
	int client_fd,
	pid_t tid,
	HTTPRequest *http_request,
	HTTPResponse *http_response
) {

	// URL could contain :// as prefix to the host and port
	// reject those requests
	// TODO: put this in the url functions?
	if (memmem(
		http_request->path, 
		strlen(http_request->path), 
		"://", 
		3
	) != NULL) {
		printfid("MEMMEM fail", tid);
		return -1;
	}

	// return 0 and tunnel is open
	// user_state->... = TUNNEL
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
	char *connect_message_buffer = NULL;

	if (user_state->retries >= 3) {
		printfid("Too many retries for client: %d", tid, client_fd);
		return -1;
	}

	/*
	Need to create a predetermined table of available routes
	with metadata to support each route available (like js object)
	so that these methods can be implemented properly
	right now I just decide that JSON ends up here or there
	there is no actual routing structure

	and nothing matter-of-factly determining things about resources

	if you want to request a file resource with GET then the file will be checked
	at runtime for existence, sometimes it might exists, others maybe not

	if you want to request a PAGE resource with GET then the page will be known to exist
	before the server is compiled!?...or use a database/CMS architecture to generate/'prerender'
	pages as static HTML that can be cached by the server and served to users (and cached by the
	users, if they agree :).

	CONNECT	: return 200 and open a TUNNEL (don't close connection)
	DELETE	: return 405 (nothing to delete...yet)
	GET		: return 200...if you can :)
	HEAD	: return 405
	OPTIONS	: return 405
	PATCH	: return 405
	POST	: return 405
	PUT	 	: return 405
	TRACE	: return 200 if resource available
	*/
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

				user_state->state = CHECK_HEADERS;

				break;
			case CHECK_HEADERS:
				printfid("HEADERS:\n%s", tid, user_state->http_request->header_storage);

				char *transfer_encoding = ht_get(
					user_state->http_request->headers, 
					HT_STR("Transfer-Encoding")
				);

				char *content_length = ht_get(
					user_state->http_request->headers,
					HT_STR("Content-Length")
				);

				if (transfer_encoding != NULL && content_length != NULL) {
					// error per http/1.1
					// TODO:
					// unless I decide to forward the message
					// in which case the content-length header is to be removed
					// and instead just process the transfer-encoding
					return -1;
				}

				user_state->state = CHECK_REQUEST_METHOD;

				break;
			case CHECK_REQUEST_METHOD:

				#define check(s) strcmp(user_state->http_request->method, s) == 0

				if (check("GET")) {
					user_state->state = GET;
				} else if (check("CONNECT")) {
					user_state->state = CONNECT;
					user_state->skip_timer = 1;
				} else if (check("POST")) {
					// anything where there is a body expected should move to MOVE_BODY
					// POST should get its own case?
					// NO anything that doesnt need its own 'thing' (like POST)
					// for retrieving body should just move to MOVE_BODY
					// and then later we can #check(...) again for POST etc and then give specific case?
					// or just reuse functions in multiple places with same call...no
					user_state->state = MOVE_BODY;
				} // ...

				#undef check

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
			/*
			case POST:
				
				break;
			*/
			case CONNECT:

				result = handle_connect_request(
					client_fd,
					tid,
					user_state->http_request,
					user_state->http_response
				);

				if (result < 0) {
					return -1;
				}

				char message[JSON_BUF_SIZE];
				int message_length;

				message_length = snprintf(
					message,
					sizeof(message),
					"HTTP/1.1 %d %s\r\n"
					"Connection: keep-alive\r\n"
					"\r\n",
					200,
					http_status_str(200)
				);

				// Any 200 response indicates that the connection
				// will become a tunnel immediately after responding.
				send_wrapper(&client_fd, message, message_length);

				user_state->state = TUNNEL;
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
			case TUNNEL:
			
				// just stay here :)
				// until the client closes the connection.

				// should be scanning for messages from the client
				// recv recv recv recv recv, free the worker if no data

				#define CONNECT_BYTES_SIZE 128
				char bytes_received[CONNECT_BYTES_SIZE];
				ssize_t recv_result;

				// try to receive some bytes
				printf("Trying to receive bytes\n");
				recv_result = recv(client_fd, bytes_received, CONNECT_BYTES_SIZE, 0);
				if (recv_result <= 0) {
					return CONNECT_CONTINUE;
				}
				
				// could save messages by writing to file per connection
				// or just read, act, forget.
				printfid("BYTES FROM CLIENT:\n--------\n%s--------", tid, bytes_received);

				// send a random message back
				char massage[JSON_BUF_SIZE] = "success\n";
				send_wrapper(&client_fd, massage, strlen(massage));

				// I will just memmem for a kill signal.

				if (1) {
					printf("CONNECT_CONTINUE\n");
					return CONNECT_CONTINUE;
				}

				if (2) {
					// stop skipping timer and delete user
					// in next cleanup cycle
					printf("skip timer reset\n");
					user_state->skip_timer = 0;
					user_state->state = FIN;
				}

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
			case ERROR: // not used
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
	int client_fd, n, ectl, epoll_result, fd, hr_result, *tid_p;
	pid_t tid;
	UserState *us = {0};

	ev.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
	tid = syscall(SYS_gettid);
	tid_p = &tid;

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
				fd = events[n].data.fd;

				// get information from hash table if available (state)
				pthread_mutex_lock(&wd->lock);
				us = ht_get(wd->user_states, HT_INT(fd));
				pthread_mutex_unlock(&wd->lock);

				if (us == NULL) {
					us = nus(fd);

					if (us != NULL) {
						// save immediatley
						pthread_mutex_lock(&wd->lock);
						ht_set(wd->user_states, HT_INT(fd), us);
						pthread_mutex_unlock(&wd->lock);
					}
				}

				if (us != NULL) {

					pthread_mutex_lock(&us->mutex);

					if (us->speed.start.tv_nsec == 0 && us->speed.start.tv_sec == 0) {
						ps_cap(&us->speed.start);
					}

					us->http_response->status = 200;
					hr_result = handle_request(fd, tid, us);

					switch (hr_result) {
						case RETRY_ERROR:
							us->retries++;
						case CONNECT_CONTINUE:
							pthread_mutex_unlock(&us->mutex);
							ev.data.fd = fd;
							epoll_ctl(wd->epc, EPOLL_CTL_MOD, fd, &ev);
							continue;
							break;
					}

					if (hr_result != 0) {
						// NO response has been sent yet.
						send_json_response(
							&fd,
							500, // should use the us->http_response->status
							"{"
								"\"error\": \"Failed to handle request\","
								"\"success\": false"
							"}"
						);
					}

					ps_cap(&us->speed.end);
					ps_print_elapsed(&us->speed, tid_p);
					pthread_mutex_unlock(&us->mutex);

					pthread_mutex_lock(&wd->lock);
					ht_remove(wd->user_states, HT_INT(fd));
					pthread_mutex_unlock(&wd->lock);
					
					free_user_state(us);
				}

				epoll_ctl(wd->epc, EPOLL_CTL_DEL, fd, NULL);
				close(fd);
			}
		}
	}

	printfid("Worker Exiting", tid);
}