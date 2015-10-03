#ifndef INCLUDE_PROTURA_DUMP_MEM_H
#define INCLUDE_PROTURA_DUMP_MEM_H

#include <protura/types.h>

void dump_mem(char *output, ksize_t outlen,
              const void *buf, ksize_t len, uint32_t base_addr);

#endif
