#!/bin/bash
# Argument one: The built kernel will all of it's symbols
# Argument two: The second-pass symbol table source
# Argument three: Where to place temporary files

PREFIX="symbol-table"
tmp_table=$3/test_symbol_table.c

# Store the second-pass symbol table in the test results
cp $2 $3/

# Generate a temporary source file for the new kernel
./scripts/symbol_table.sh $1 > $tmp_table

cmp -s -- $tmp_table $2
assert_success "Symbol table diff is below:" diff --minimal -u $tmp_table $2
