#ifndef PAGING_H
#define PAGING_H

#include "types.h"

#define PAGE_SIZE 4096
#define PAGE_ENTRIES 1024
#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2

void init_paging();

int paging_is_enabled();

u32 paging_get_directory_address();
u32 paging_get_first_table_address();

u32 paging_get_directory_index(u32 address);
u32 paging_get_table_index(u32 address);
u32 paging_get_page_offset(u32 address);

int paging_is_mapped(u32 address);

#endif