#!/bin/bash

PROTURA_OBJ=$1

ERR_FILE=$(mktemp)

# This dumps out the ".initcall" section's binary contents.  The section itself
# is a bunch of NUL terminated strings, one right after another, each
# containing a dependency pair. The NULs are converted into newlines, resulting
# in a long stream of dependency pairs
#
# The pairs are then sorted into an order that satisfies the required dependences
# via `tsort`, which does a topological sort of the dependency tree.
#
initcalls=$(objdump --section=.discard.initcall.deps --full-contents $PROTURA_OBJ | grep '^ *[0-9a-f]'  | xxd -r | tr '\0' '\n' | tsort 2>$ERR_FILE)

ERR=$(cat $ERR_FILE)
rm $ERR_FILE

# Errors from tsort generally indicate a cycle in the input
if ! [ -z "$ERR" ]
then
    echo "tsort reported an error!" 1>&2
    echo "$ERR" 1>&2

    exit 1
fi

echo
echo "#include <protura/stddef.h>"
echo "#include <protura/initcall.h>"
echo

for i in $initcalls
do
    echo "extern void __init_$i(void);"
done

echo
echo "void (*initcalls[]) (void) = {"

for i in $initcalls
do
    echo "    __init_$i,"
done

echo "    NULL"
echo "};"

