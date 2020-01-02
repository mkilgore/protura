#!/bin/bash

make extra-gcc -j5
CODE=$?

echo "GCC extra build status code: $CODE"

if [ $CODE -ne 0 ]; then
    echo "GCC EXTRA BUILD ERROR!"
fi

exit $CODE
