#!/bin/bash

cd ./query_control

make

./bin/query_control query.config FIFO_1 4 & sleep 0.01 & ./bin/query_control query.config FIFO_2 3 & sleep 0.01 & ./bin/query_control query.config FIFO_3 5 & sleep 0.01 & ./bin/query_control query.config FIFO_4 1


