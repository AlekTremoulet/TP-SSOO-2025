#!/bin/bash

cd ./query_control
make

./bin/query_control query.config STORAGE_1 0
./bin/query_control query.config STORAGE_2 2
./bin/query_control query.config STORAGE_3 4
./bin/query_control query.config STORAGE_4 6


