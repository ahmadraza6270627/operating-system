#include "heap.h"

typedef struct heap_block {
    u32 size;
    u32 free;
    struct heap_block *next;
} heap_block_t;

static heap_block_t *heap_first_block = 0;

static u32 align4(u32 value) {
    if (value & 3) {
        value = (value & 0xFFFFFFFC) + 4;
    }

    return value;
}

static void heap_merge_free_blocks() {
    heap_block_t *block;

    block = heap_first_block;

    while (block != 0 && block->next != 0) {
        if (block->free && block->next->free) {
            block->size = block->size + sizeof(heap_block_t) + block->next->size;
            block->next = block->next->next;
        } else {
            block = block->next;
        }
    }
}

void init_heap() {
    heap_first_block = (heap_block_t *)HEAP_START;

    heap_first_block->size = HEAP_END - HEAP_START - sizeof(heap_block_t);
    heap_first_block->free = 1;
    heap_first_block->next = 0;
}

void *heap_alloc(u32 size) {
    heap_block_t *block;
    heap_block_t *new_block;
    u32 aligned_size;
    u32 i;
    u8 *memory;

    if (size == 0) {
        return 0;
    }

    aligned_size = align4(size);
    block = heap_first_block;

    while (block != 0) {
        if (block->free && block->size >= aligned_size) {
            if (block->size >= aligned_size + sizeof(heap_block_t) + 4) {
                new_block = (heap_block_t *)((u32)block + sizeof(heap_block_t) + aligned_size);

                new_block->size = block->size - aligned_size - sizeof(heap_block_t);
                new_block->free = 1;
                new_block->next = block->next;

                block->next = new_block;
                block->size = aligned_size;
            }

            block->free = 0;

            memory = (u8 *)((u32)block + sizeof(heap_block_t));

            for (i = 0; i < block->size; i++) {
                memory[i] = 0;
            }

            return (void *)memory;
        }

        block = block->next;
    }

    return 0;
}

int heap_free(void *ptr) {
    heap_block_t *block;
    u32 payload_address;

    if (ptr == 0) {
        return 0;
    }

    block = heap_first_block;

    while (block != 0) {
        payload_address = (u32)block + sizeof(heap_block_t);

        if (payload_address == (u32)ptr) {
            if (block->free) {
                return 0;
            }

            block->free = 1;
            heap_merge_free_blocks();

            return 1;
        }

        block = block->next;
    }

    return 0;
}

void heap_reset() {
    init_heap();
}

u32 heap_get_start() {
    return HEAP_START;
}

u32 heap_get_end() {
    return HEAP_END;
}

u32 heap_get_total() {
    return HEAP_END - HEAP_START;
}

u32 heap_get_used() {
    heap_block_t *block;
    u32 used = 0;

    block = heap_first_block;

    while (block != 0) {
        if (!block->free) {
            used += block->size;
        }

        block = block->next;
    }

    return used;
}

u32 heap_get_free() {
    heap_block_t *block;
    u32 free_bytes = 0;

    block = heap_first_block;

    while (block != 0) {
        if (block->free) {
            free_bytes += block->size;
        }

        block = block->next;
    }

    return free_bytes;
}

int heap_get_block_count() {
    heap_block_t *block;
    int count = 0;

    block = heap_first_block;

    while (block != 0) {
        count++;
        block = block->next;
    }

    return count;
}

int heap_get_block_info(int index, u32 *address, u32 *size, int *is_free) {
    heap_block_t *block;
    int current = 0;

    block = heap_first_block;

    while (block != 0) {
        if (current == index) {
            *address = (u32)block + sizeof(heap_block_t);
            *size = block->size;
            *is_free = block->free ? 1 : 0;
            return 1;
        }

        current++;
        block = block->next;
    }

    return 0;
}