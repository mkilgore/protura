#!/bin/perl -w

# This generates the asm code for each irq
# It's extremely repetitive, because we have to have a unique entry-point for
# every irq, but we really want to use a unified handler. So to fix this, we
# have a 'stub' entry-point, which just pushes the irq number onto the stack
# (And a zero in the case of no error code) and then calls a unitifed handler
# which eventually delegates this down to the proper irq handler.

print "# Generated code - from ./irq_array_gen.pl\n";
print ".globl irq_handler\n";
for (my $i = 0; $i < 256; $i++) {
    print ".globl irq$i\n";
    print "irq$i:\n";
    if (!($i == 8 || ($i >= 10 && $i <= 14))) {
        print "  pushl \$0\n";
    }
    print "  pushl \$$i\n";
    print "  jmp irq_handler\n";
}

print "\n# 'irq_hands' array\n";
print ".data\n";
print ".globl irq_hands\n";
print "irq_hands:\n";

for (my $i = 0; $i < 256; $i++) {
    print "  .long irq$i\n";
}

