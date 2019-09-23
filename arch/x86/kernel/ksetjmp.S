#
# Copyright (C) 2019 Matt Kilgore
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License v2 as published by the
# Free Software Foundation.
#

# These are implements of `setjmp` and `longjmp` for use in the kernel
# The primary usecase is `ktest`, which can use it to return back to the test
# runner in the event of a failed assert.

# These implementations take advantage of the caller-saved regisers of x86,
# which are %eax, %ecx, and %edx - we can freely use these as the caller will
# restore them for us.

.globl ksetjmp
ksetjmp:
    # Store the pointer to `struct x86_jmpbuf` into %eax
    movl 4(%esp), %eax

    # Store the registers into `struct x86_jmpbuf`
    # Note that we're taking advantage of the caller-saved registers
    # to only have to save a few
    movl %ebx, (%eax)
    movl %esi, 4(%eax)
    movl %edi, 8(%eax)
    movl %ebp, 12(%eax)
    movl %esp, 16(%eax)

    # Save the return address we're going to return too
    movl (%esp), %edx
    movl %edx, 20(%eax)

    # Return zero
    xorl %eax, %eax
    ret

.globl klongjmp
klongjmp:
    # Store the pointer to `struct x86_jmpbuf` into %eax
    movl 4(%esp), %edx

    # The return value is stored in the second argument
    # Grab it and place it in %eax
    movl 8(%eax), %eax

    # Force the return value to 1 if it is zero
    testl %eax, %eax
    jnz 0f
    incl %eax
0:
    # Restore the registers
    movl (%edx), %ebx
    movl 4(%edx), %esi
    movl 8(%edx), %edi
    movl 12(%edx), %ebp
    movl 16(%edx), %esp

    # Restore the return address
    movl 20(%edx), %ecx
    movl %ecx, (%esp)

    ret
