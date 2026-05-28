package main

import (
	"bytes"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"
)

// chunkedReader wraps a reader and forces chunked encoding
// by implementing io.Reader without a known Content-Length.
type chunkedReader struct {
	r    io.Reader
	size int
}

func (c *chunkedReader) Read(p []byte) (n int, err error) {
	return c.r.Read(p)
}

func main() {
	serverURL := "http://localhost:3000" // Change to your target server

	// Create 2000 bytes of payload
	payload := strings.Repeat("A", 2000)
	body := bytes.NewBufferString(payload)

	// Build the request
	req, err := http.NewRequest(http.MethodPost, serverURL, body)
	if err != nil {
		fmt.Printf("Error creating request: %v\n", err)
		return
	}

	// Set headers
	req.Header.Set("Content-Type", "application/octet-stream")
	req.Header.Set("Transfer-Encoding", "chunked")

	// Force chunked encoding:
	// Setting ContentLength to -1 (unknown) makes Go's HTTP client
	// automatically use Transfer-Encoding: chunked (HTTP/1.1 only).
	req.ContentLength = -1

	// Explicitly set HTTP/1.1 (default, but be explicit)
	req.Proto = "HTTP/1.1"
	req.ProtoMajor = 1
	req.ProtoMinor = 1
	// req.ContentLength = 2000 // makes request work, but my server should reject requests that include both transfer-encoding and content-length unlike http/1.0

	// Use a client that disables HTTP/2 to guarantee HTTP/1.1
	client := &http.Client{
		Timeout: 15 * time.Second,
		Transport: &http.Transport{
			ForceAttemptHTTP2: false, // stay on HTTP/1.1
			DisableKeepAlives: false,
		},
	}

	fmt.Printf("Sending HTTP/1.1 POST with chunked Transfer-Encoding...\n")
	fmt.Printf("Target : %s\n", serverURL)
	fmt.Printf("Payload: %d bytes\n\n", len(payload))

	resp, err := client.Do(req)
	if err != nil {
		fmt.Printf("Request error: %v\n", err)
		return
	}
	defer resp.Body.Close()

	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		fmt.Printf("Error reading response: %v\n", err)
		return
	}

	fmt.Printf("Response status : %s\n", resp.Status)
	fmt.Printf("Response proto  : %s\n", resp.Proto)
	fmt.Printf("Response body   : %s\n", string(respBody))
}
