---
layout: post
title: "ktest"
parent: APIs
grand_parent: Documentation
author: "Matt Kilgore"
---

ktest
=====
{: .no_toc }

ktest is a unit testing framework for the Protura kernel. It provides an easy way to run a test suite against part of the kernel after it has booted but before user-space starts.

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

Makefile Usage
--------------

The easiest way to run the ktest test suite is via `make`. To run all tests one
can run `make check-kernel`. That will run all the existing ktest tests on a
variety of machines (The machine definitions can be found in
`./tests/testcases/ktest/`). It is also possible to run `make debug-ktest`
which will run the same tests as `make check-kernel`, but will start the
debugging environment and break on the first failed test. Both commands accept
a few arguments:

| Parameter | Description |
| --- | --- |
| MODULE | The name of the ktest module to run. All other modules will be skipped. |
| SINGLE_TEST| The name of a single test to run. All testcases will be run, all other tests are skipped |

Kernel Parameters
-----------------

| Parameter | Default | Description |
| --- | --- | --- |
| `ktest.run` | `false` | When `true`, all tests will be after the kernel has booted |
| `ktest.reboot_after_run` | `false` | When `true`, the system will be rebooted after the tests have run. This is useful for implementing a CI framework, so that the system is automatically killed after the test run is over |
| `ktest.module` | none | When set, only module(s) with a matching name as the one provided will be run |
| `ktest.single_test` | none | When set, only test(s) with a matching name as the one provided will be run |

API Usage
---------

The core of ktest is two structures:

```c
struct ktest;

struct ktest_unit {
    void (*test) (struct ktest *);
    const char *name;
    struct ktest_arg args[7];
};

struct ktest_module {
    const char *name;
    const struct ktest_unit *tests;
    size_t test_count;
    /* ... */
};
```

As might be clear from those definitions, a complete test suite for part of the kernel is represented by a `struct ktest_module`. The module then points to an array of `struct ktest_unit` objects, each of which represents a test to run and return a pass/fail. Both of these things have names to identify them. For a module, it may be something simple like "kbuf" or "slab". For a unit test, it would be more specific like "kbuf-printf-overflow". Unit's can also be provided arguments, which is covered later. Also note `struct ktest`, which is provided to the unit tests as an opaque pointer. This stores state required by the test runner, and is provided to functions/macros to get things like arguments and do assertions.

To define a test suite, it will look approximately like this:

```c
static void example_one(struct ktest *kt) { }
static void example_two(struct ktest *kt) { }

static const struct ktest_unit kbuf_test_units[] = {
    KTEST_UNIT("example-test-1", example_one),
    KTEST_UNIT("example-test-2", example_two),
};

KTEST_MODULE_DEFINE("example-test-suite", example_test_units);
```

Note that everything is `static` and all the test information is `const` - this is required. The `KTEST_UNIT()` macro should be used to create `struct ktest_unit` entries, and the `KTEST_MODULE_DEFINE()` macro should be used for creating the `struct ktest_module`.

### Assertions

A few different assertion types are included. But with that, instead of having many different assertion functions for every type, the arguments to some of the assertion functions are type-checked via `_Generic` to produce better test output. The current list of assertion functions are below. Note that for all of the assertions, the "actual" should be the thing under test, and the "expected" should be the value you're comparing against.

| Assertion | Description |
| --- | --- |
| `ktest_assert_equal(kt, expected, actual)` | Compares two values and passes if they are the same. The type of these arguments are used to determine how to compare and display these values |
| `ktest_assert_notequal(kt, expected, actual)` | Compares two values and passes if they are different. The type of these arguments are used to determine how to compare and display these values |
| `ktest_assert_equal_str(kt, expected, actual)` | Compares two string values and passes if they are the same. Both arguments should be NUL terminated strings |
| `ktest_assert_equal_mem(kt, expected, actual, len)` | Compares two byte arrays of `len` length and passes if they are the same. Both buffers should be at least `len` bytes in size |
| `ktest_assert_fail(kt, fmt, ..)` | Always fails. Display a `printf`-style formatted message |


### Unit Test Cases

The `KTEST_UNIT` macro allows defining multiple test cases for a single test function. This works by providing sets of arguments (described in the next section) to the `KTEST_UNIT` macro, one for each test case you want to run. The argument sets are wrapped in parenthesis, thus it looks like this:

```c
static const struct ktest_unit kbuf_test_units[] = {
    KTEST_UNIT("example-test-1", example_one
        (KT_INT(0)),
        (KT_INT(1)),
        (KT_INT(2))),

    KTEST_UNIT("example-test-2", example_two
        (KT_INT(0), KT_STR("arg2")),
        (KT_INT(1), KT_STR("arg2-2")),
        (KT_INT(200), KT_STR("arg2-3"))),
};
```

### Unit Test Arguments

Arguments to a unit test can be provided to a test via the `KT_*` macros. Current ones are:

| Macro | Type | Notes |
| --- | --- | --- |
| `KT_INT` | `int` | |
| `KT_UINT` | `unsigned int` | |
| `KT_STR` | `const char *` | |
| `KT_USER_BUF` | `struct user_buffer` | Creates a buffer with `is_user` set to true |
| `KT_KERNEL_BUF` | `struct user_buffer` | Creates a buffer with `is_user` set to false |

These macros are provided as part of test cases provided to `KTEST_UNIT`. The arguments can then be acquired in the test via the `KT_ARG(kt, idx, type)` macro, which takes the `struct ktest *`, the index of the argument, and the type of the argument. All together it looks like this:

```c
static void example_test(struct ktest *kt)
{
    int arg1 = KT_ARG(kt, 0, int);
    const char *arg2 = KT_ARG(kt, 1, const char *);
}

static const struct ktest_unit kbuf_test_units[] = {
    KTEST_UNIT("example-test1", example_test,
        (KT_INT(100), KT_STR("foo")),
        (KT_INT(200), KT_STR("bar"))),
};
```

That example defines two test cases for the provided function. They pass an integer and a string as arguments, and then inside the test `KT_ARG` is used to get the value of these arguments. Note that the types of the provided arguments are checked for consistency with the type provided to `KT_ARG`, though the checking happens at runtime and will fail your test, rather than fail to compile.

### Module Setup and Teardown functions

When defining a `struct ktest_module`, you can provide two functions to be called before any of the module's test are run, and after the module's tests are completed. You can also provide two additional setup and teardown functions to run before and after each test is run. These are all provided directly to the `KTEST_MODULE_DEFINE` as optional additional arguments. The module setup/teardown come first, and either both or neither arguments have to be provided (Though `NULL` can be provided if the function isn't needed). An example is below:

```c
static int module_setup(struct ktest_module *mod)
{
    /* ... */
}

static int module_teardown(struct ktest_module *mod)
{
    /* ... */
}

static int test_setup(struct ktest *kt)
{
    /* ... */
}

static const struct ktest_unit example_units[] = {
};

/* NULL has to be provided for test_teardown because a test_setup function is provided */
KTEST_MODULE_DEFINE("example-module", example_units, module_setup, module_teardown, test_setup, NULL);
```

### Module-scoped private information

Combined with module setup and teardown functions, it can be useful to share things between tests that might be a bit heavy to do in each test (Ex. Allocated scratch pieces of memory, or stuctures, etc.). This can be done via the `priv` field on the `struct module`. It can be accessed directly in your module setup and teardown functions, and also accessed through your `struct ktest *` pointer in your tests via `ktest_get_mod_priv()`. A simple example is below:

```c
struct mod_priv_info {
    int v1;
};

static void test(struct ktest *kt)
{
    struct mod_priv_info *info = ktest_get_mod_priv(kt);
    /* ... */
}

static int module_setup(struct ktest_module *mod)
{
    struct mod_priv_info *info = kmalloc(sizeof(*info), PAL_KERNEL);
    info->v1 = 20;
    mod->priv = info;
}

static int module_teardown(struct ktest_module *mod)
{
    struct mod_priv_info *info = mod->priv;
    kfree(info);
}

static const struct ktest_unit example_units[] = {
    KTEST_UNIT("Test", test),
};

KTEST_MODULE_DEFINE("example-module", example_units, module_setup, module_teardown);
```
