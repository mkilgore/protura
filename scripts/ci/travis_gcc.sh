#!/bin/bash

git submodule update --init --depth 1 --recursive ./disk/toolchain

TOOL_CHAIN_HASH=$(cd ./disk/toolchain; git rev-parse HEAD:./gcc)
CACHED_TOOLCHAIN_HASH=

if [ -e ./toolchain/hash ]; then
    CACHED_TOOLCHAIN_HASH=$(cat ./toolchain/hash)
fi

echo "GCC hash: $TOOL_CHAIN_HASH"
echo "CACHED hash: $CACHED_TOOLCHAIN_HASH"

MAKE_CMD=toolchain

if [ "$TOOL_CHAIN_HASH" == "$CACHED_TOOLCHAIN_HASH" ]; then
    echo "Using cached GCC, just building newlib..."
    MAKE_CMD=rebuild-newlib
fi

make $MAKE_CMD -j3 >./toolchain.log 2>&1 &
PID=$!

while [ -d /proc/$PID ]
do
    echo "Building Toolchain..."
    sleep 10s
done

wait $PID
CODE=$?

echo "Toolchain build status code: $CODE"

if [ $CODE -ne 0 ]; then
    echo "TOOLCHAIN BUILD ERROR:"
    cat ./toolchain.log
fi

echo $TOOL_CHAIN_HASH > ./toolchain/hash
exit $CODE
