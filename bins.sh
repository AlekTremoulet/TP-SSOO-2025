#!/bin/bash

rm ./query_control/bin/query_control
rm ./master/bin/master
rm ./worker/bin/worker
rm ./storage/bin/storage


cd  ./query_control/ && make  &&
cd ..
cd  ./master/ && make  &&
cd ..
cd  ./storage/ && make &&
cd ..
cd  ./worker/ && make 