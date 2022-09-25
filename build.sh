#! /bin/bash

command g++ -c coroutine/coctx_swap.S 
command ar -crv libcoctx.a coctx_swap.o

if [ "$1" == "echo" ]; then
    command g++ echo/*.cpp net/*.cpp coroutine/*.cpp -L. -lcoctx -std=c++17 -pthread -g -o echoserver
elif [ "$1" == "http" ]; then
     command g++ http/*.cpp net/*.cpp coroutine/*.cpp -L. -lcoctx -std=c++17 -pthread -g -o httpserver
else
    echo "selectable param : echo , http"
fi