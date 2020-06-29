#!/bin/bash

nm $1 -S --defined-only | sort | ./scripts/symbol_table.pl
