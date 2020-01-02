#!/bin/bash

HASH_FILE=./bin/toolchain/hash

git submodule update --init --depth 1 --recursive ./toolchain

TOOL_CHAIN_HASH=$(cd ./toolchain; git rev-parse HEAD:./gcc)
CACHED_TOOLCHAIN_HASH=

if [ -e $HASH_FILE ]; then
    CACHED_TOOLCHAIN_HASH=$(cat $HASH_FILE)
fi

echo "GCC hash: $TOOL_CHAIN_HASH"
echo "CACHED hash: $CACHED_TOOLCHAIN_HASH"

MAKE_CMD=toolchain

if [ "$TOOL_CHAIN_HASH" == "$CACHED_TOOLCHAIN_HASH" ]; then
    echo "Using cached GCC, just building newlib..."
    MAKE_CMD=rebuild-newlib
fi

make $MAKE_CMD -j5
CODE=$?

echo "Toolchain build status code: $CODE"

if [ $CODE -ne 0 ]; then
    echo "TOOLCHAIN BUILD ERROR!"
fi

echo $TOOL_CHAIN_HASH > $HASH_FILE
exit $CODE
