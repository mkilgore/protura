#ifndef INCLUDE_PROTURA_DRIVERS_MEM_H
#define INCLUDE_PROTURA_DRIVERS_MEM_H

extern struct file_ops mem_file_ops;

enum {
    MEM_MINOR_ZERO,
    MEM_MINOR_FULL,
    MEM_MINOR_NULL,
};

extern struct file_ops mem_zero_file_ops;
extern struct file_ops mem_full_file_ops;
extern struct file_ops mem_null_file_ops;

#endif
