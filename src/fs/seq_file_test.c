/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
/*
 * Tests for kbuf.c - included directly at the end of kbuf.c
 */

#include <protura/types.h>
#include <protura/fs/seq_file.h>
#include <protura/ktest.h>

struct test_state {
    struct ktest *kt;
    int count;
};

static int test_start_err(struct seq_file *seq)
{
    return -EBADF;
}

static int test_start_state(struct seq_file *seq)
{
    struct test_state *state = seq->priv;
    state->count++;

    if (seq->iter_offset == 20)
        flag_set(&seq->flags, SEQ_FILE_DONE);

    return 0;
}

static void test_end_state(struct seq_file *seq)
{
    struct test_state *state = seq->priv;
    state->count--;
}

static int test_render(struct seq_file *seq)
{
    return seq_printf(seq, "this is a very very very very very very very very long line!\n");
}

static int test_next(struct seq_file *seq)
{
    seq->iter_offset++;
    if (seq->iter_offset == 20)
        flag_set(&seq->flags, SEQ_FILE_DONE);

    return 0;
}

static void seq_file_fill_test(struct ktest *kt)
{
    struct test_state state;
    memset(&state, 0, sizeof(state));

    struct seq_file_ops ops = {
        .start = test_start_state,
        .next = test_next,
        .render = test_render,
        .end = test_end_state,
    };
    struct seq_file seq;
    seq_file_init(&seq, &ops);
    seq.priv = &state;

    ktest_assert_equal(kt, 0, seq.iter_offset);
    int ret = __seq_file_fill(&seq);

    ktest_assert_equal(kt, 0, ret);
    ktest_assert_equal(kt, 0, state.count);
    ktest_assert_equal(kt, 20, seq.iter_offset);

    /* Call it again and verify the iter_offset stays in the same place */
    ret = __seq_file_fill(&seq);
    ktest_assert_equal(kt, 20, seq.iter_offset);

    seq_file_clear(&seq);
}

static void seq_file_empty_test(struct ktest *kt)
{
    struct test_state state;
    memset(&state, 0, sizeof(state));

    struct seq_file_ops ops = {
        .start = test_start_err,
        .next = test_next,
        .render = test_render,
        .end = test_end_state,
    };
    struct seq_file seq;
    seq_file_init(&seq, &ops);
    seq.priv = &state;

    int ret = __seq_file_fill(&seq);

    ktest_assert_equal(kt, -EBADF, ret);

    seq_file_clear(&seq);
}

static const struct ktest_unit seq_file_test_units[] = {
    KTEST_UNIT_INIT("seq-file-empty-test", seq_file_empty_test),
    KTEST_UNIT_INIT("seq-file-fill-test", seq_file_fill_test),
};

static const __ktest struct ktest_module seq_file_test_module
    = KTEST_MODULE_INIT("seq-file", seq_file_test_units);
