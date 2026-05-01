#ifndef IDT_H
#define IDT_H

#include "types.h"

#define IDT_ENTRIES 256
#define KERNEL_CS 0x08

#define low_16(address) (u16)((address) & 0xFFFF)
#define high_16(address) (u16)(((address) >> 16) & 0xFFFF)

typedef struct {
    u16 low_offset;
    u16 sel;
    u8 always0;
    u8 flags;
    u16 high_offset;
} __attribute__((packed)) idt_gate_t;

typedef struct {
    u16 limit;
    u32 base;
} __attribute__((packed)) idt_register_t;

extern idt_gate_t idt[IDT_ENTRIES];
extern idt_register_t idt_reg;

void set_idt_gate(int n, u32 handler);
void set_idt();

#endif