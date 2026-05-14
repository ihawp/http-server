#!/bin/bash

gcc -iquote ./src/include src/*.c -o build/server -lpthread

valgrind -s -v --leak-check=full --track-origins=yes --show-leak-kinds=all ./build/server 3000 > valgrind.txt 2>&1