# HTTP/1.1 Server in C

## Useful Resources

- [HTTP/1.1](https://datatracker.ietf.org/doc/rfc9112/)
- [HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110)
- [HTTP Caching](https://www.rfc-editor.org/rfc/rfc9111.html)
- [Security Considerations](https://datatracker.ietf.org/doc/rfc9931/)
- [getaddrinfo (w/UDP example)](https://man7.org/linux/man-pages/man3/getaddrinfo.3.html)
- [epoll (w/usage example)](https://www.man7.org/linux/man-pages/man7/epoll.7.html)
- [pthread](https://man7.org/linux/man-pages/man7/pthreads.7.html)
- [pthread_mutex_init](https://man7.org/linux/man-pages/man3/pthread_mutex_lock.3.html)
- [Threads](https://cs341.cs.illinois.edu/coursebook/Threads)
- [Hash Table Data Structure](https://www.geeksforgeeks.org/dsa/hash-table-data-structure/)
- Hash Table Implementation: [Repository](https://github.com/benhoyt/ht) & [Article](https://benhoyt.com/writings/hash-table-in-c/)
- [Path Traversal](https://owasp.org/www-community/attacks/Path_Traversal)
- [Hexadecimal](https://en.wikipedia.org/wiki/Hexadecimal)
- [Bitwise Operations + etc](https://www.ewskills.com/embedded-c/bitwise-operations)
- [Response Splitting](https://owasp.org/www-community/attacks/HTTP_Response_Splitting)

## About

This implementation uses *[sys/epoll.h](https://www.man7.org/linux/man-pages/man7/epoll.7.html)* and *[pthread.h](https://man7.org/linux/man-pages/man7/pthreads.7.html)* to create worker threads that monitor multiple client file descriptors for 'readiness' using **[epoll_wait(...)](https://man7.org/linux/man-pages/man2/epoll_wait.2.html)**.

### Workers

When a new client connects the connection is accepted with **[accept(...)](https://man7.org/linux/man-pages/man2/accept.2.html)**. If the `client_fd` is successfully created it is added to the epoll monitoring list, where it will eventually be marked as ready. When a client file descriptor is marked as ready it is passed to **handle_request(...)** where it is treated as an HTTP/1.1 request. After the request is handled the file descriptor is removed from the epoll monitoring list. If a client's request cannot be completed on this cycle their `UserState` is saved to a hash table where their `client_fd` is the key.

### State Machine Architecture

To prevent malicious and slow requests from hanging the server, a state machine approach was chosen over `SO_RCVTIMEO` (socket receive timeout). Both `O_NONBLOCK` and `SO_RCVTIMEO` cause `recv(...)` to return `-1` and set `errno` to `EAGAIN` or `EWOULDBLOCK` when data isn't ready, making them indistinguishable at the call site.

`handle_request(...)` uses a switch statement to decide what to do with the client's current state. Where the client's current state is defined in 'their' `UserState` struct. This struct also holds any heap allocated data pertaining to the client's connection to the server, including the `HTTPRequest` and `HTTPResponse` structs.

The timeout is currently set to `5000ms` for development, though you'd expect something in the range of `50ms` to `500ms` in a real production environment.

Retries happen as the client fails to send data (causing EAGAIN or EWOULDBLOCK). When this happens the client is moved back into the epoll waitlist using `epoll_ctl(...)`/`EPOLL_CTL_MOD` and their `UserState` data is placed in a hash table that is accessible by the `http_worker(...)` threads and the main thread.

If the client reaches 3 retries, or they don't send all their data before the deadline, any data allocated for their request is freed, they are sent a `JSON` response indicating a failure, and their connection is closed.

#### Why use a timeout?

Well, sometimes clients lie. If a client tells me (whole-heartedly) that they are going to send 2004 octets (by denoting `Content-Length: 2004` in the headers of their request), but they only ever send 1 octet, then the server will hang and wait forever for the client to send the rest of their bytes, which will likely never arrive.

### HTTP Lifecycle

When a request is made to the server and the client file descriptor has eventually been marked as ready, the HTTP lifecycle begins.

Since headers are expected at the beginning of any content sent by the client, we start by receiving chunks with **recv_header_chunks(...)** until we find the start of body indicator (**\r\n\r\n**), or something goes wrong (No data available, client closed connection, etc).

Once we find the start of body indicator we can safely assume two things. One, we have received all of the available headers for the request, and two, we may have received the beginning of the body already. The latter only matters if the user is attempting to send a body as part of their request (i.e not a GET, DELETE ... request).

When the latter is the case, we use the difference between the pointer at the beginning of the header string and the pointer to the **\r\n\r\n** sequence to determine how much of the body has already been read. Once determined, the pointer pointing at the **\r\n\r\n** sequence is incremented by 4 to move past the sequence to the beginning of the body text. We then use **recv_body_chunks(...)** to receive the rest of the bytes until the **http_request->content_length** is reached or exceeded.

### Headers

Headers are stored in a hash table (per [HTTP/1.1](https://datatracker.ietf.org/doc/rfc9112/)), allowing for O(1) average lookup time per key. 

Previously, headers were stored inside a `LIMArray` as a `LineInMemory` struct, which contained a pointer to the start of the string and a count outlining how long the string is, so retrieving a specific header required a linear scan through all entries to find a match.

The Content-Length is read into memory as a long using **[strtol(...)](https://man7.org/linux/man-pages/man3/strtol.3.html)** during the **find_headers(...)** call. This value is stored in the **HTTPRequest** struct.

It is safe to assume that the **HTTPRequest** struct will store data from the user's request, while the **HTTPResponse** struct will store hints about how to respond to the user's request, such as the status code.

### GET

GET requests are served from the */public* folder using **send_stream_file(...)**, which streams the file to the client in chunks using Chunked Transfer-Encoding.

Before the file is opened, the path goes through two checks. **decode_url(...)** decodes any percent-encoded characters (e.g. `%20` → ` `), and **sanitize_path(...)** rejects paths containing `..`, double slashes, or spaces to mitigate path traversal attacks. If either check fails the request is rejected.

Path traversal is not fully solved; a more robust approach would reconstruct the path internally rather than sanitizing the client-provided string directly:

```
// for safety you could split apart the path 
// and rebuild it to a hidden internal structure
// ...
```

### StringView

Adapted from Tsoding's video on why C strings are terrible. Watch that video [here](https://www.youtube.com/watch?v=y8PLpDgZc0E).