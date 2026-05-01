#include "paging.h"

static u32 page_directory[PAGE_ENTRIES] __attribute__((aligned(PAGE_SIZE)));
static u32 first_page_table[PAGE_ENTRIES] __attribute__((aligned(PAGE_SIZE)));

static int paging_enabled = 0;

static void load_page_directory(u32 page_directory_address) {
    asm volatile("mov %0, %%cr3" : : "r"(page_directory_address));
}

static void enable_paging_register() {
    u32 cr0;

    asm volatile("mov %%cr0, %0" : "=r"(cr0));

    cr0 = cr0 | 0x80000000;

    asm volatile("mov %0, %%cr0" : : "r"(cr0));
}

void init_paging() {
    int i;

    for (i = 0; i < PAGE_ENTRIES; i++) {
        page_directory[i] = 0;
    }

    for (i = 0; i < PAGE_ENTRIES; i++) {
        first_page_table[i] = (i * PAGE_SIZE) | PAGE_PRESENT | PAGE_RW;
    }

    page_directory[0] = ((u32)first_page_table) | PAGE_PRESENT | PAGE_RW;

    load_page_directory((u32)page_directory);
    enable_paging_register();

    paging_enabled = 1;
}

int paging_is_enabled() {
    return paging_enabled;
}

u32 paging_get_directory_address() {
    return (u32)page_directory;
}

u32 paging_get_first_table_address() {
    return (u32)first_page_table;
}

u32 paging_get_directory_index(u32 address) {
    return address >> 22;
}

u32 paging_get_table_index(u32 address) {
    return (address >> 12) & 0x3FF;
}

u32 paging_get_page_offset(u32 address) {
    return address & 0xFFF;
}

int paging_is_mapped(u32 address) {
    u32 directory_index;
    u32 table_index;

    directory_index = paging_get_directory_index(address);
    table_index = paging_get_table_index(address);

    if (directory_index != 0) {
        return 0;
    }

    if ((page_directory[directory_index] & PAGE_PRESENT) == 0) {
        return 0;
    }

    if ((first_page_table[table_index] & PAGE_PRESENT) == 0) {
        return 0;
    }

    return 1;
}