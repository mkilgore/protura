#ifndef COMMON_DUMP_MEM_H
#define COMMON_DUMP_MEM_H

#include <stdlib.h>
#include <stdint.h>

void dump_mem(const void *buf, size_t len, uint32_t base_addr);

#endif
