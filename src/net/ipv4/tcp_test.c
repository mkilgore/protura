/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
/*
 * Tests for basic TCP utilities
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/list.h>
#include <protura/ktest.h>

#include <protura/net/socket.h>
#include <protura/net/ipv4/tcp.h>
#include <protura/net/ipv4/ipv4.h>
#include <protura/net.h>
#include "ipv4.h"
#include "tcp.h"

static void tcp_seq_after_test(struct ktest *kt)
{
    uint32_t seq1 = KT_ARG(kt, 0, uint32_t);
    uint32_t seq2 = KT_ARG(kt, 1, uint32_t);
    int result = KT_ARG(kt, 2, int);

    ktest_assert_equal(kt, result, tcp_seq_after(seq1, seq2));
}

static void tcp_seq_before_test(struct ktest *kt)
{
    uint32_t seq1 = KT_ARG(kt, 0, uint32_t);
    uint32_t seq2 = KT_ARG(kt, 1, uint32_t);
    int result = KT_ARG(kt, 2, int);

    ktest_assert_equal(kt, result, tcp_seq_before(seq1, seq2));
}

static void tcp_seq_between_test(struct ktest *kt)
{
    uint32_t seq1 = KT_ARG(kt, 0, uint32_t);
    uint32_t seq2 = KT_ARG(kt, 1, uint32_t);
    uint32_t seq3 = KT_ARG(kt, 2, uint32_t);
    int result = KT_ARG(kt, 3, int);

    ktest_assert_equal(kt, result, tcp_seq_between(seq1, seq2, seq3));
}

static const struct ktest_unit tcp_test_units[] = {
    KTEST_UNIT("tcp-after", tcp_seq_after_test, KT_UINT(0), KT_UINT(0), KT_INT(0)),
    KTEST_UNIT("tcp-after", tcp_seq_after_test, KT_UINT(1), KT_UINT(0), KT_INT(1)),
    KTEST_UNIT("tcp-after", tcp_seq_after_test, KT_UINT(0), KT_UINT(1), KT_INT(0)),
    KTEST_UNIT("tcp-after", tcp_seq_after_test, KT_UINT(-1), KT_UINT(0), KT_INT(0)),
    KTEST_UNIT("tcp-after", tcp_seq_after_test, KT_UINT(0), KT_UINT(-1), KT_INT(1)),

    KTEST_UNIT("tcp-before", tcp_seq_before_test, KT_UINT(0), KT_UINT(0), KT_INT(0)),
    KTEST_UNIT("tcp-before", tcp_seq_before_test, KT_UINT(1), KT_UINT(0), KT_INT(0)),
    KTEST_UNIT("tcp-before", tcp_seq_before_test, KT_UINT(0), KT_UINT(1), KT_INT(1)),
    KTEST_UNIT("tcp-before", tcp_seq_before_test, KT_UINT(-1), KT_UINT(0), KT_INT(1)),
    KTEST_UNIT("tcp-before", tcp_seq_before_test, KT_UINT(0), KT_UINT(-1), KT_INT(0)),

    KTEST_UNIT("tcp-seq-between", tcp_seq_between_test, KT_UINT(0),   KT_UINT(0),  KT_UINT(0),  KT_INT(0)),
    KTEST_UNIT("tcp-seq-between", tcp_seq_between_test, KT_UINT(0),   KT_UINT(0),  KT_UINT(1),  KT_INT(0)),
    KTEST_UNIT("tcp-seq-between", tcp_seq_between_test, KT_UINT(0),   KT_UINT(1),  KT_UINT(0),  KT_INT(0)),
    KTEST_UNIT("tcp-seq-between", tcp_seq_between_test, KT_UINT(1),   KT_UINT(0),  KT_UINT(0),  KT_INT(0)),
    KTEST_UNIT("tcp-seq-between", tcp_seq_between_test, KT_UINT(0),   KT_UINT(1),  KT_UINT(2),  KT_INT(1)),
    KTEST_UNIT("tcp-seq-between", tcp_seq_between_test, KT_UINT(-1),  KT_UINT(1),  KT_UINT(2),  KT_INT(1)),
    KTEST_UNIT("tcp-seq-between", tcp_seq_between_test, KT_UINT(-2),  KT_UINT(20), KT_UINT(22), KT_INT(1)),
    KTEST_UNIT("tcp-seq-between", tcp_seq_between_test, KT_UINT(-20), KT_UINT(-4), KT_UINT(0),  KT_INT(1)),
};

KTEST_MODULE_DEFINE("tcp", tcp_test_units);
