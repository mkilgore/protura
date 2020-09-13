#!/bin/bash

PROTURA_OBJ=$1

# This dumps out the ".initcall" section's binary contents.
# The section itself is a bunch of NUL terminated strings, one right after another, each containing a dependency pair.
# The NULs are converted into newlines, resulting in a long stream of dependency pairs
#
# The pairs are then output in dot format
#
initcalls=$(objdump --section=.discard.initcall.deps --full-contents $PROTURA_OBJ | grep '^ *[0-9a-f]'  | xxd -r | tr '\0' '\n')

echo "digraph initcall {"

echo "$initcalls" | while read init1 init2
do
    ([ -z "$init1" ] || [ -z "$init2" ]) && continue

    echo "$init1 -> $init2;"
done

echo "}"

