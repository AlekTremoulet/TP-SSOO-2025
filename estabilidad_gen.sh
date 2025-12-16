#!/bin/bash

#./
echo "Estabilidad General"

for k in $(seq 1 4);
do
   for i in $(seq 1 25);
    do
    echo "./query_control/bin/query_control ./query_control/query.config ./queries/AGING_${k} 20" 
    ./query_control/bin/query_control ./query_control/query.config ./queries/AGING_${k} 20 &
    done
done

echo "Finalizo" 