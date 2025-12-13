#!/bin/bash

cd ./query_control

make

./bin/query_control query.config FIFO_1 4
./bin/query_control query.config FIFO_2 3
./bin/query_control query.config FIFO_3 5
./bin/query_control query.config FIFO_4 1


