#include "isr.h"
#include "idt.h"
#include "ports.h"
#include "../drivers/screen.h"

#define PIC1            0x20
#define PIC2            0xA0
#define PIC1_COMMAND    PIC1
#define PIC1_DATA       (PIC1 + 1)
#define PIC2_COMMAND    PIC2
#define PIC2_DATA       (PIC2 + 1)
#define PIC_EOI         0x20

#define ICW1_ICW4       0x01
#define ICW1_INIT       0x10
#define ICW4_8086       0x01

static isr_t interrupt_handlers[256];

char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "Floating Point Exception",
    "Alignment Check",
    "Machine Check",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

static void print_uint_local(u32 value) {
    char str[16];
    int i = 0;
    int j;
    char temp;

    if (value == 0) {
        kprint("0");
        return;
    }

    while (value > 0) {
        str[i] = (value % 10) + '0';
        value = value / 10;
        i++;
    }

    str[i] = '\0';

    for (j = 0; j < i / 2; j++) {
        temp = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = temp;
    }

    kprint(str);
}

static char hex_digit_local(u32 value) {
    value = value & 0xF;

    if (value < 10) {
        return '0' + value;
    }

    return 'A' + (value - 10);
}

static void print_hex32_local(u32 value) {
    char str[11];
    int i;

    str[0] = '0';
    str[1] = 'x';

    for (i = 0; i < 8; i++) {
        str[2 + i] = hex_digit_local(value >> ((7 - i) * 4));
    }

    str[10] = '\0';

    kprint(str);
}

static void halt_after_exception() {
    asm volatile("cli");

    while (1) {
        asm volatile("hlt");
    }
}

void PIC_remap(u32 offset1, u32 offset2) {
    u8 a1;
    u8 a2;

    a1 = port_byte_in(PIC1_DATA);
    a2 = port_byte_in(PIC2_DATA);

    port_byte_out(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    port_byte_out(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

    port_byte_out(PIC1_DATA, offset1);
    port_byte_out(PIC2_DATA, offset2);

    port_byte_out(PIC1_DATA, 0x04);
    port_byte_out(PIC2_DATA, 0x02);

    port_byte_out(PIC1_DATA, ICW4_8086);
    port_byte_out(PIC2_DATA, ICW4_8086);

    port_byte_out(PIC1_DATA, a1);
    port_byte_out(PIC2_DATA, a2);
}

void isr_install() {
    set_idt_gate(0, (u32)isr0);
    set_idt_gate(1, (u32)isr1);
    set_idt_gate(2, (u32)isr2);
    set_idt_gate(3, (u32)isr3);
    set_idt_gate(4, (u32)isr4);
    set_idt_gate(5, (u32)isr5);
    set_idt_gate(6, (u32)isr6);
    set_idt_gate(7, (u32)isr7);
    set_idt_gate(8, (u32)isr8);
    set_idt_gate(9, (u32)isr9);
    set_idt_gate(10, (u32)isr10);
    set_idt_gate(11, (u32)isr11);
    set_idt_gate(12, (u32)isr12);
    set_idt_gate(13, (u32)isr13);
    set_idt_gate(14, (u32)isr14);
    set_idt_gate(15, (u32)isr15);
    set_idt_gate(16, (u32)isr16);
    set_idt_gate(17, (u32)isr17);
    set_idt_gate(18, (u32)isr18);
    set_idt_gate(19, (u32)isr19);
    set_idt_gate(20, (u32)isr20);
    set_idt_gate(21, (u32)isr21);
    set_idt_gate(22, (u32)isr22);
    set_idt_gate(23, (u32)isr23);
    set_idt_gate(24, (u32)isr24);
    set_idt_gate(25, (u32)isr25);
    set_idt_gate(26, (u32)isr26);
    set_idt_gate(27, (u32)isr27);
    set_idt_gate(28, (u32)isr28);
    set_idt_gate(29, (u32)isr29);
    set_idt_gate(30, (u32)isr30);
    set_idt_gate(31, (u32)isr31);

    PIC_remap(0x20, 0x28);

    set_idt_gate(32, (u32)irq0);
    set_idt_gate(33, (u32)irq1);
    set_idt_gate(34, (u32)irq2);
    set_idt_gate(35, (u32)irq3);
    set_idt_gate(36, (u32)irq4);
    set_idt_gate(37, (u32)irq5);
    set_idt_gate(38, (u32)irq6);
    set_idt_gate(39, (u32)irq7);
    set_idt_gate(40, (u32)irq8);
    set_idt_gate(41, (u32)irq9);
    set_idt_gate(42, (u32)irq10);
    set_idt_gate(43, (u32)irq11);
    set_idt_gate(44, (u32)irq12);
    set_idt_gate(45, (u32)irq13);
    set_idt_gate(46, (u32)irq14);
    set_idt_gate(47, (u32)irq15);

    set_idt();
}

void isr_handler(registers_t *r) {
    clear_screen();

    kprint_colored("CPU EXCEPTION\n", LIGHT_RED_ON_BLACK);

    kprint("Exception number: ");
    print_uint_local(r->int_no);
    kprint("\n");

    kprint("Exception name: ");

    if (r->int_no < 32) {
        kprint(exception_messages[r->int_no]);
    } else {
        kprint("Unknown");
    }

    kprint("\n");

    kprint("Error code: ");
    print_hex32_local(r->err_code);
    kprint("\n");

    kprint("EIP: ");
    print_hex32_local(r->eip);
    kprint("\n");

    kprint("CS: ");
    print_hex32_local(r->cs);
    kprint("\n");

    kprint("EFLAGS: ");
    print_hex32_local(r->eflags);
    kprint("\n");

    kprint("\nSystem halted after CPU exception.\n");

    halt_after_exception();
}

void add_interrupt_handler(u8 n, isr_t handler) {
    interrupt_handlers[n] = handler;
}

void irq_handler(registers_t *r) {
    if (r->int_no >= 40) {
        port_byte_out(PIC2_COMMAND, PIC_EOI);
    }

    port_byte_out(PIC1_COMMAND, PIC_EOI);

    if (interrupt_handlers[r->int_no] != 0) {
        isr_t handler = interrupt_handlers[r->int_no];
        handler(r);
    }
}

void irq_install() {
    asm volatile("sti");
}