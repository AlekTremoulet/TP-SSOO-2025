#!/bin/bash

cd ./query_control
make

./bin/query_control query.config AGING_1 4 & ./bin/query_control query.config AGING_2 3 & ./bin/query_control query.config AGING_3 5 & ./bin/query_control query.config AGING_4 1


