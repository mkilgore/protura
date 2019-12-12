/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_USER_CHECK_H
#define INCLUDE_ARCH_USER_CHECK_H

int user_check_access(struct user_buffer data, size_t size);

static inline int user_memcpy_from_kernel(struct user_buffer data, const void *krnl, size_t size)
{
    int ret = user_check_access(data, size);
    if (ret)
        return ret;

    ret = -EFAULT;
    struct task *__current = cpu_get_local()->current;
    flag_set(&__current->flags, TASK_FLAG_RW_USER);

    switch (size) {
    case 1:
        asm volatile(
                "movl $1f, %[jmp_addr]\n"
                "movb %[krnl], %[usr]\n"
                "movl $0, %[ret]\n"
                "1:\n"
                : [jmp_addr] "=m" (__current->user_check_jmp_address), [ret] "=r" (ret)
                : [krnl] "q" (*(const uint8_t *) krnl), [usr] "m" (*(uint8_t *)data.ptr), "1" (ret)
                : "memory", "cc"
                );
        break;

    case 2:
        asm volatile(
                "movl $1f, %[jmp_addr]\n"
                "movw %[krnl], %[usr]\n"
                "movl $0, %[ret]\n"
                "1:\n"
                : [jmp_addr] "=m" (__current->user_check_jmp_address), [ret] "=r" (ret)
                : [krnl] "q" (*(const uint16_t *) krnl), [usr] "m" (*(uint16_t *)data.ptr), "1" (ret)
                : "memory", "cc"
                );
        break;

    case 4:
        asm volatile(
                "movl $1f, %[jmp_addr]\n"
                "movl %[krnl], %[usr]\n"
                "movl $0, %[ret]\n"
                "1:\n"
                : [jmp_addr] "=m" (__current->user_check_jmp_address), [ret] "=r" (ret)
                : [krnl] "q" (*(const uint32_t *) krnl), [usr] "m" (*(uint32_t *)data.ptr), "1" (ret)
                : "memory", "cc"
                );
        break;

    default:
        asm volatile(
                "movl $1f, %[jmp_addr]\n"
                "rep movsb\n"
                "movl $0, %[ret]\n"
                "1:\n"
                : [jmp_addr] "=m" (__current->user_check_jmp_address), [ret] "=r" (ret), "=S" (krnl), "=D" (data.ptr), "=c" (size)
                : "2" (krnl), "3" (data.ptr), "4" (size), "1" (ret)
                : "memory", "cc"
                );
        break;
    }

    flag_clear(&__current->flags, TASK_FLAG_RW_USER);
    __current->user_check_jmp_address = NULL;
    return ret;
}

static inline int user_memcpy_to_kernel(void *krnl, struct user_buffer data, size_t size)
{
    uint32_t user_ptr = (uint32_t)data.ptr;

    int ret = user_check_access(data, size);
    if (ret)
        return ret;

    ret = -EFAULT;
    struct task *__current = cpu_get_local()->current;
    flag_set(&__current->flags, TASK_FLAG_RW_USER);

    switch (size) {
    case 1:
        asm volatile(
                "movl $1f, %[jmp_addr]\n"
                "movb %[usr], %[krnl]\n"
                "movl $0, %[ret]\n"
                "1:\n"
                : [jmp_addr] "=m" (__current->user_check_jmp_address), [ret] "=r" (ret), [krnl] "=q" (*(uint8_t *)krnl)
                : [usr] "m" (*(const uint8_t *) data.ptr), "1" (ret)
                : "memory", "cc"
                );
        break;

    case 2:
        asm volatile(
                "movl $1f, %[jmp_addr]\n"
                "movw %[usr], %[krnl]\n"
                "movl $0, %[ret]\n"
                "1:\n"
                : [jmp_addr] "=m" (__current->user_check_jmp_address), [ret] "=r" (ret), [krnl] "=q" (*(uint16_t *)krnl)
                : [usr] "m" (*(const uint16_t *) data.ptr), "1" (ret)
                : "memory", "cc"
                );
        break;

    case 4:
        asm volatile(
                "movl $1f, %[jmp_addr]\n"
                "movl %[usr], %[krnl]\n"
                "movl $0, %[ret]\n"
                "1:\n"
                : [jmp_addr] "=m" (__current->user_check_jmp_address), [ret] "=r" (ret), [krnl] "=q" (*(uint32_t *)krnl)
                : [usr] "m" (*(const uint32_t *) data.ptr), "1" (ret)
                : "memory", "cc"
                );
        break;

    default:
        asm volatile(
                "movl $1f, %[jmp_addr]\n"
                "rep movsb\n"
                "movl $0, %[ret]\n"
                "1:\n"
                : [jmp_addr] "=m" (__current->user_check_jmp_address), [ret] "=r" (ret), "=D" (krnl), "=S" (user_ptr), "=c" (size)
                : "2" (krnl), "3" (user_ptr), "4" (size), "1" (ret)
                : "memory", "cc"
                );
        break;
    }

    flag_clear(&__current->flags, TASK_FLAG_RW_USER);
    __current->user_check_jmp_address = NULL;
    return ret;
}

static inline int user_memset_from_kernel(struct user_buffer data, char ch, size_t size)
{
    uint32_t user_ptr = (uint32_t)data.ptr;

    int ret = user_check_access(data, size);
    if (ret)
        return ret;

    ret = -EFAULT;
    struct task *__current = cpu_get_local()->current;
    flag_set(&__current->flags, TASK_FLAG_RW_USER);

    asm volatile(
            "movl $1f, %[jmp_addr]\n"
            "rep stosb\n"
            "movl $0, %[ret]\n"
            "1:\n"
            : [jmp_addr] "=m" (__current->user_check_jmp_address), [ret] "=r" (ret), "=D" (user_ptr), "=c" (size)
            : "2" (user_ptr), "3" (size), "1" (ret), "a" (ch)
            : "memory", "cc"
            );

    flag_clear(&__current->flags, TASK_FLAG_RW_USER);
    __current->user_check_jmp_address = NULL;
    return ret;
}

#endif
