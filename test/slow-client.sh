#!/bin/bash

DATA=$(python3 -c "print('a' * 2000)")

curl 127.0.0.1:3000 \
    --verbose \
    -X POST \
    --limit-rate 700 \
    --path-as-is \
    -H "Content-Length: 2000" \
    -d "$DATA" \