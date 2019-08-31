#!/bin/bash

if ! [ -x "$(command -v git)" ]; then
  echo "-nongit"
  exit 1
fi

LAST_TAG=$(git describe --tags --abbrev=0 HEAD)
COMMIT_COUNT=$(git rev-list $LAST_TAG.. --count)
COMMIT_HASH=$(git rev-parse --short HEAD)
git diff --no-ext-diff --quiet --exit-code
DIRTY=$?

VERSION=

if [ "$COMMIT_COUNT" != "0" ]; then
    VERSION+="-$COMMIT_COUNT-$COMMIT_HASH"
fi

if [ "$DIRTY" != "0" ]; then
    VERSION+="-dirty"
fi

echo $VERSION
