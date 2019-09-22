#!/usr/bin/perl -w
use strict;
use warnings;

foreach my $line (<STDIN>) {
    chomp($line);
    if ($line =~ /^\s*$/) {
        next;
    }

    my @str = split(':', $line, 2);

    my $timestatus = $str[0];
    my $text = $str[1];
    my $test_count;

    # Parse the line [ddddd][L] into the time and log level
    $timestatus =~ /\[(\d+)\]\[([a-zA-Z]+)\]/;
    my $time = $1;
    my $status = $2;

    if ($status eq "PANIC") {
        print "TESTS FAILED: PANIC\n";
        exit 1;
    }

    if ("$text" =~ /==== Full test run: FAIL -> (\d+) ====/) {
        $test_count = $1;
        print "Total test errors: $test_count\n";
        exit 1;
    }
}

print "All tests passed!\n";
exit 0;
