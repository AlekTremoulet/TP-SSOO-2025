#!/bin/bash

cd ./query_control
make

./bin/query_control query.config ESCRITURA_ARCHIVO_COMMITED 1 & sleep 0.05
./bin/query_control query.config FILE_EXISTENTE 1 & sleep 0.05
./bin/query_control query.config LECTURA_FUERA_DEL_LIMITE 1 & sleep 0.05
./bin/query_control query.config TAG_EXISTENTE 1

wait


