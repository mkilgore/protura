#!/bin/bash

make toolchain -j3 >./toolchain.log 2>&1 &
PID=$!

while [ -d /proc/$PID ]
do
    echo "Building GCC..."
    sleep 10s
done

wait $PID
CODE=$?

echo "GCC build status code: $CODE"

if [ $CODE -ne 0 ]; then
    echo "GCC BUILD ERROR:"
    cat ./toolchain.log
fi

exit $CODE
