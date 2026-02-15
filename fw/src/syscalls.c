/**
 * Minimal newlib syscall implementations for this firmware.
 * Routes stdout/stderr to USART1 and provides _sbrk for malloc.
 */

#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>

#include "stm32c0xx.h"

/* Provide _sbrk using linker symbols defined in the project's linker script */
void *_sbrk(ptrdiff_t incr)
{
    extern uint8_t _end; /* defined in linker script */
    extern uint8_t _estack; /* top of RAM */
    extern uint32_t _Min_Stack_Size; /* reserved stack size */

    static uint8_t *heap_end = NULL;
    uint8_t *prev_heap_end;

    const uint32_t stack_limit = (uint32_t)&_estack - (uint32_t)&_Min_Stack_Size;
    const uint8_t *max_heap = (uint8_t *)stack_limit;

    if (heap_end == NULL) {
        heap_end = &_end;
    }

    /* Prevent heap from growing into the reserved stack area */
    if (heap_end + incr > max_heap) {
        errno = ENOMEM;
        return (void *)-1;
    }

    prev_heap_end = heap_end;
    heap_end += incr;
    return (void *)prev_heap_end;
}

/* Simple write: route stdout/stderr to USART1 (blocking) */
int _write(int fd, const char *buf, int len)
{
    (void)fd; /* support stdout/stderr only */
    for (int i = 0; i < len; ++i) {
        /* Wait for TXE */
        while (!(USART1->ISR & USART_ISR_TXE_TXFNF)) ;
        USART1->TDR = (uint8_t)buf[i];
    }
    return len;
}

/* Simple read: support stdin from USART1 (blocking) */
int _read(int fd, char *buf, int len)
{
    (void)fd;
    int i = 0;
    for (; i < len; ++i) {
        /* Wait for RXNE */
        while (!(USART1->ISR & USART_ISR_RXNE_RXFNE)) ;
        buf[i] = (char)(USART1->RDR & 0xFF);
    }
    return i;
}

int _close(int fd)
{
    (void)fd;
    errno = EBADF;
    return -1;
}

int _lseek(int fd, int ptr, int dir)
{
    (void)fd; (void)ptr; (void)dir;
    return -1;
}

int _fstat(int fd, struct stat *st)
{
    (void)fd;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int fd)
{
    (void)fd;
    return 1;
}
