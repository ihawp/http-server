#!/bin/bash

(
  echo -e "POST / HTTP/1.1\r\n"
  echo -e "Host: 127.0.0.1:3000\r\n"
  echo -e "Content-Type: application/octet-stream\r\n"
  echo -e "Content-Length: 2000\r\n"
  echo -e "\r"
  for i in $(seq 1 20); do
    printf 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' # 100 bytes
    sleep 0.5
  done
) | nc 127.0.0.1 3000