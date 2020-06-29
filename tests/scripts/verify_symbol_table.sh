#!/bin/bash
# Argument one: The built kernel will all of it's symbols
# Argument two: The second-pass symbol table source
# Argument three: Where to place temporary files

. ./tests/scripts/colors.sh

tmp_table=$3/test_symbol_table.c

# Store the second-pass symbol table in the test results
cp $2 $3/

# Generate a temporary source file for the new kernel
./scripts/symbol_table.sh $1 > $tmp_table

if ! cmp -s -- $tmp_table $2
then
    echo "Symbol Table Check: ${RED}FAILED!$RESET"
    echo "Symbol table diff is below:"
    diff --minimal $tmp_table $2
    exit 1
fi

echo "Symbol Table Check: ${GREEN}PASSED!$RESET"
