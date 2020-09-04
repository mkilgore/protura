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

    # Parse the line [ddddd.ddd][L] into the time and log level
    $timestatus =~ /\[(\d+.\d+)\]\[([a-zA-Z]+)\]/;
    my $time = $1;
    my $status = $2;

    if ($status eq "PANIC") {
        exit 1;
    }

    if ("$text" =~ /==== Full test run: FAIL -> (\d+) ====/) {
        exit 1;
    }

    if ("$text" =~ /==== Full test run: PASS ====/) {
        exit 0;
    }
}

exit 1;
