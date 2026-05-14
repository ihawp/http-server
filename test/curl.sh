#!/bin/bash

curl 127.0.0.1:3000// \
    --verbose \
    -X POST \
    --path-as-is \
    -H "Content-Length: 2090" \
    -d h \