#!/bin/perl -w
use strict;
use warnings;

foreach my $line (<STDIN>) {
    chomp($line);
    if (substr($line, 0, 9) eq '#define E') {
        my @str = split(' ', $line);
        my $id = $str[1];

        print "ERR($id),\n"
    }
}

