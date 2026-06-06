#!/usr/bin/env bash
# connect_loop.sh — CONNECT to 127.0.0.1:3000, then POST data every 10 seconds

TARGET="127.0.0.1:3000"
INTERVAL=10
COUNTER=0

echo "Connecting to $TARGET..."

while true; do
  COUNTER=$((COUNTER + 1))
  TIMESTAMP=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
  PAYLOAD="{\"seq\": $COUNTER, \"ts\": \"$TIMESTAMP\", \"msg\": \"heartbeat\"}"

  HTTP_CODE=$(curl \
    --silent \
    --show-error \
    --output /dev/null \
    --write-out "%{http_code}" \
    --request CONNECT \
    --url "http://$TARGET/" \
    --header "Host: 127.0.0.1" \
    --header "Connection: keep-alive" \
    --data "$PAYLOAD")

  echo "[$TIMESTAMP] seq=$COUNTER  status=$HTTP_CODE  payload=$PAYLOAD"

  sleep "$INTERVAL"
done