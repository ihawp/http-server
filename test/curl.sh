#!/bin/bash

# no space in "Content-Length:2090"
curl 127.0.0.1:3000// --verbose -X POST --path-as-is -H "Content-Length:2090" -d h

# this is fine since the send goes nowhere 
# since connection is closed by server after 
# the h is received
# curl localhost:3000// -X POST --path-as-is -H "Content-Length:1" -d hi 
