#!/bin/bash

cd ./query_control

make

./bin/query_control query.config MEMORIA_WORKER 0

wait
