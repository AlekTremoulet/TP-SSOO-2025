#!/bin/bash

#./
echo "Estabilidad General"

cd ./query_control
make

for i in $(seq 1 25);
do
./bin/query_control query.config AGING_1 20 &
done

for i in $(seq 1 25);
do
./bin/query_control query.config AGING_2 20 &
done

for i in $(seq 1 25);
do
./bin/query_control query.config AGING_3 20 &
done

for i in $(seq 1 25);
do
./bin/query_control query.config AGING_4 20 &
done

echo "Finalizo"
