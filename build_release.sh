#!/bin/bash
mkdir -p build 
rm -rf build/miniob/ 
cd build 

cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_ASAN=ON -DWITH_UNIT_TESTS=OFF
make 