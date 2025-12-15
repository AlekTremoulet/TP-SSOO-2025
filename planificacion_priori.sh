#!/bin/bash

cd ./query_control
make

./bin/query_control query.config AGING_1 4 & sleep 0.01 & ./bin/query_control query.config AGING_2 3 & sleep 0.01 & ./bin/query_control query.config AGING_3 5 & sleep 0.01 & ./bin/query_control query.config AGING_4 1


