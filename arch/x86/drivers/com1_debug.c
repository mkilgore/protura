
#include <protura/types.h>
#include <protura/stdarg.h>
#include <protura/basic_printf.h>

#include <arch/asm.h>

#define PORT 0x3F8

void com1_init(void)
{
    outb(PORT + 1, 0x00);    // Disable all interrupts
    outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(PORT + 1, 0x00);    //                  (hi byte)
    outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

void com1_putchar(char c)
{
    outb(PORT, c);
}

void com1_putnstr(const char *s, size_t len)
{
    size_t l;
    for (l = 0; l < len; l++)
        com1_putchar(s[l]);
}

static void com1_printf_putchar(struct printf_backbone *b, char ch)
{
    com1_putchar(ch);
}

static void com1_printf_putnstr(struct printf_backbone *b, const char *s, size_t len)
{
    com1_putnstr(s, len);
}

void com1_printfv(const char *s, va_list lst)
{
    struct printf_backbone backbone = {
        .putchar = com1_printf_putchar,
        .putnstr = com1_printf_putnstr,
    };

    basic_printfv(&backbone, s, lst);
}

void com1_printf(const char *fmt, ...)
{
    va_list lst;
    va_start(lst, fmt);
    com1_printfv(fmt, lst);
    va_end(lst);
}

