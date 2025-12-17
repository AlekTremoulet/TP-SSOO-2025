#!/bin/bash

cd ./query_control
make

./bin/query_control query.config STORAGE_1 0 & sleep 0.1
./bin/query_control query.config STORAGE_2 2 & sleep 0.1
./bin/query_control query.config STORAGE_3 4 & sleep 0.1
./bin/query_control query.config STORAGE_4 6 & sleep 0.1

wait
