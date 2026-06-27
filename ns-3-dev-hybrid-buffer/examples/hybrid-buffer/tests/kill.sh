#!/bin/bash
xx=$1

ps -ef | grep "$xx"

if [ "$#" -eq 2 ]; then
    kill=$2
    kill -9 $(ps -ef | grep "$xx" | awk '{print $2}')
fi

