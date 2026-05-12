#include <stdio.h>
#include "state_machine.h"

/*

What does my state machine need to know about?

My state machine needs to be able to remember
any data already retrieved from the request.

It is NOT safe to assume that the state machine will
ONLY be required for POST and other BODY INCLUDING
methods. The state machine should be able to be used if
the client fails to send headers in recv_header_chunks(...)
on a GET request.

Currently, the HTTPRequest and HTTPResponse used to track users requests
is a single instance for each worker, as each worker handles one client
at a time. To make the HTTPRequset and HTTPResponse more useful for the

So having a *http_request, instead of a http_request, could leak memory
from other users requests (if not handled properly), but I will handle it
properly.

SO we allocate some memory somewhere for each users HTTPRequest
and HTTPResponse structs, now we can safely reference them in the StateMachine
struct generated when the user has failed to send us some data after (a while).

Why does this state machine need to exist?

I believe it needs to exist because I have reached a wall in what is possible
with socket.h/epoll.h in regards to determining when a client fails to send data,
or would block, or will send more data, or or or, while knowing that they are GOING
TO FAIL to reach the Content-Length.

If a request is sent with a content-length of 2, and only 1 octet is ever sent, the
worker will hang, and other workers will hang on seperate subsequent requests. This happens
because the server is expecting more data because SOCKNOBLOCK said EAGAIN and EWOULDBLOCK, but
RCVTIMEO also says EAGAIN and EWOULDBLOCK, and both set the result of recv(...) to -1.

*/

// GOODBYE STACK ALLOCATED HTTPRequest AND HTTPResponse
// there are plenty of stack allocated items within HTTPRequest,
// method, content length, path, http_version

/*

And then the whole process has access to a hash table
of StateMachine structs, where each struct contains the info
for the (key) client_fd.

*/