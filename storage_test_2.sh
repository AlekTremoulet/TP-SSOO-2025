#!/bin/bash

cd ./query_control
make

./bin/query_control query.config STORAGE_5 0

wait
