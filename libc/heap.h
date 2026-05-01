#ifndef HEAP_H
#define HEAP_H

#include "../cpu/types.h"

#define HEAP_START 0x100000
#define HEAP_END   0x200000

void init_heap();

void *heap_alloc(u32 size);
int heap_free(void *ptr);
void heap_reset();

u32 heap_get_start();
u32 heap_get_end();
u32 heap_get_used();
u32 heap_get_free();
u32 heap_get_total();

int heap_get_block_count();
int heap_get_block_info(int index, u32 *address, u32 *size, int *is_free);

#endif