#!/bin/perl -w

# Generates aseembly for the initial page-table entry
# We generate 4 tables, for 16MB of memory for the initial boot stages. Once
# paging is enabled in paging_init, all of kernel memory get's mapped so this
# table isn't used anymore.

print "# Generated code - from ./gen_initial_pagedir.pl\n";
print "\n";
print "#include <arch/paging.h>\n";
print "\n";

# This loops for 4 page-tables. Each page-table gets 1024 entries
# A page-table totals to mapping 4MB of memory, so 4 of them maps the 16MB's we
# want.
print ".set x, 0\n";
print ".align 0x1000\n";
for (my $i = 0; $i < 4; $i++) {
    print ".globl pg$i\n";

    print "pg$i:\n";
    print ".rept 1024\n";
    print ".long ((x << 12) | PTE_PRESENT | PTE_WRITABLE)\n";
    print ".set x, x + 1\n";
    print ".endr\n";
}



