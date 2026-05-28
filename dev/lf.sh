#!/bin/bash

find . -name "*.sh" -exec sed -i 's/\r$//' {} +