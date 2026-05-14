#!/bin/bash

echo "Bad client #1 (malicious)"
curl 127.0.0.1:3000// \
    --verbose \
    -X POST \
    --path-as-is \
    -H "Content-Length: 2090" \
    -d h \

echo "Bad client #2 (slow)"
DATA=$(python3 -c "print('a' * 2000)")
curl 127.0.0.1:3000 \
    --verbose \
    -X POST \
    --limit-rate 200 \
    --path-as-is \
    -H "Content-Length:2000" \
    -d "$DATA" \

echo "Bad client #3 (slow)"
(
  echo -e "DELETE / HTTP/1.1\r\n"
  echo -e "Host: 127.0.0.1:3000\r\n"
  echo -e "Content-Type: application/octet-stream\r\n"
  echo -e "Content-Length: 2222\r\n"
  echo -e "\r"
  for i in $(seq 1 20); do
    printf 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' # 100 bytes
    sleep 0.5
  done
) | nc 127.0.0.1 3000