import socket
import gzip

# Gzip the payload
payload = b"A" * 2000
gz_payload = gzip.compress(payload)

# Chunk it manually
chunk = f"{len(payload):x}\r\n".encode() + payload + b"\r\n"
terminator = b"0\r\n\r\n"
body = chunk + terminator

request = (
    b"POST / HTTP/1.1\r\n"
    b"Host: localhost:3000\r\n"
    b"Content-Type: application/octet-stream\r\n"
    b"Transfer-Encoding: gzip, chunked\r\n"
    b"\r\n"
) + body

with socket.create_connection(("localhost", 3000)) as sock:
    sock.sendall(request)

    # Read response
    response = b""
    while True:
        chunk = sock.recv(4096)
        if not chunk:
            break
        response += chunk

print(response.decode(errors="replace"))