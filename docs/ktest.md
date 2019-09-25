ktest
=====

ktest is a unit testing framework for the Protura kernel. It provides an easy way to run a test suite against part of the kernel after it has booted but before user-space starts.

Kernel Parameters
=================

| Parameter | Default | Description |
| --- | --- | --- |
| `ktest.run` | `false` | When `true`, all tests will be after the kernel has booted |
| `ktest.reboot_after_run` | `false` | When `true`, the system will be rebooted after the tests have run. This is useful for implementing a CI framework, so that the system is automatically killed after the test run is over |

API Usage
=========

The core of ktest is two structures:

```c
struct ktest;

struct ktest_unit {
    void (*test) (struct ktest *);
    const char *name;
    struct ktest_arg args[6];
    int arg_count;
};

struct ktest_module {
    const char *name;
    const struct ktest_unit *tests;
    size_t test_count;
};
```

As might be clear from those definitions, a complete test suite for part of the kernel is represented by a `struct ktest_module`. The module then points to an array of `struct ktest_unit` objects, each of which represents a test to run and return a pass/fail. Both of these things have names to identify them. For a module, it may be something simple like "kbuf" or "slab". For a unit test, it would be more specific like "kbuf-printf-overflow". Unit's can also be provided arguments, which is covered later. Also note `struct ktest`, which is provided to the unit tests as an opaque pointer. This stores state required by the test runner, and is provided to functions/macros to get things like arguments and do assertions.

To define a test suite, it will look approximately like this:

```c
static void example_one(struct ktest *kt) { }
static void example_two(struct ktest *kt) { }

static const struct ktest_unit kbuf_test_units[] = {
    KTEST_UNIT_INIT("example-test-1", example_one),
    KTEST_UNIT_INIT("example-test-2", example_two),
};

static const struct ktest_module __ktest example_test_module = {
    .name = "example",
    .tests = example_test_units,
    .test_count = ARRAY_SIZE(example_test_units),
};
```

Note that everything is `static` and all the test information is `const` - this is required. In addition, the `__ktest` attribute must be placed on the module - this will place the module into the `.ktest` section so that it can be discovered at runtime. The `KTEST_UNIT_INIT()` macro should be used to create `struct ktest_unit` entries.

Assertions
----------

A few different assertion types are included. But with that, instead of having many different assertion functions for every type, the arguments to some of the assertion functions are type-checked via `_Generic` to produce better test output. The current list of assertion functions are below. Note that for all of the assertions, the "actual" should be the thing under test, and the "expected" should be the value you're comparing against.

| assertion | description |
| --- | --- |
| `ktest_assert_equal(kt, expected, actual)` | Compares two values and passes if they are the same. The type of these arguments are used to determine how to compare and display these values |
| `ktest_assert_notequal(kt, expected, actual)` | Compares two values and passes if they are different. The type of these arguments are used to determine how to compare and display these values |
| `ktest_assert_equal_str(kt, expected, actual)` | Compares two string values and passes if they are the same. Both arguments should be NUL terminated strings |
| `ktest_assert_equal_mem(kt, expected, actual, len)` | Compares two byte arrays of `len` length and passes if they are the same. Both buffers should be at least `len` bytes in size |
| `ktest_assert_fail(kt, fmt, ..)` | Always fails. Display a `printf`-style formatted message |

Unit Test Arguments
-------------------

Arguments to a unit test can be provided to a test via the `KT_*` macros. Current ones are:

| macro | type |
| --- | --- |
| `KT_INT` | `int` |
| `KT_UINT` | `unsigned int` |
| `KT_STR` | `const char *` |

These macros are provided as extra arguments to `KTEST_UNIT_INIT`. The arguments can then be acquired in the test via the `KT_ARG(kt, idx, type)` macro, which takes the `struct ktest *`, the index of the argument, and the type of the argument. All together it looks like this:

```c
static void example_test(struct ktest *kt)
{
    int arg1 = KT_ARG(kt, 0, int);
    const char *arg2 = KT_ARG(kt, 1, const char *);
}

struct ktest_unit unit1 = KTEST_UNIT_INIT("example-test1", example_test, KT_INT(100), KT_STR("foo");
struct ktest_unit unit2 = KTEST_UNIT_INIT("example-test2", example_test, KT_INT(200), KT_STR("bar");
```

That example includes two unit tests, both using the same function for running the test. They pass an integer and a string as arguments, and then inside the test `KT_ARG` is used to get the value of these arguments. Note that the types of the provided arguments are checked for consistency with the type provided to `KT_ARG`, though the checking happens at runtime and will fail your test, rather than fail to compile.


