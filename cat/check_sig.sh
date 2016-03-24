#!/bin/sh
#./cat /dev/urandom > /dev/null &
sleep 1
echo "Bombing with signals..."

while true
do
  kill -s USR1 `pidof cat`
done
