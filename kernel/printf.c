/*
 * Formatted console output -- printf, panic.
 * Implements %d, %x, %p, %s.
 */

#include <stdarg.h>

#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "riscv.h"
#include "defs.h"

volatile int panicked = 0;

// lock to avoid interleaving concurrent printf's.
static struct {
  struct spinlock lock;
  int locking;
} pr;

static char digits[] = "0123456789abcdef";

static void printint(int integer, int base, int sign)
{
  unsigned int x;
  char buf[16];
  int i = 0;

  if (sign && (sign = integer < 0))
    x = -integer;
  else
    x = integer;

  do {
    buf[i++] = digits[x % base];
  } while ((x /= base) != 0);

  if (sign)
    buf[i++] = '-';

  while (--i >= 0)
    console_put(buf[i]);
}

static void printptr(unsigned long x)
{
  int i;

  console_put('0');
  console_put('x');
  for (i = 0; i < (sizeof(unsigned long) * 2); i++, x <<= 4)
    console_put(digits[x >> (sizeof(unsigned long) * 8 - 4)]);
}

// Print to the console. only understands %d, %x, %p, %s.
void printf(char *fmt, ...)
{
  int i, c, locking;
  va_list ap;
  char *s;

  locking = pr.locking;
  if (locking)
    acquire(&pr.lock);

  if (fmt == 0)
    panic("null fmt");

  va_start(ap, fmt);
  for (i = 0; (c = fmt[i] & 0xff) != 0; i++) {
    if (c != '%') {
      console_put(c);
      continue;
    }

    c = fmt[++i] & 0xff;
    if (c == 0)
      break;
    switch(c) {
    case 'd':
      printint(va_arg(ap, int), 10, 1);
      break;
    case 'x':
      printint(va_arg(ap, int), 16, 1);
      break;
    case 'p':
      printptr(va_arg(ap, unsigned long));
      break;
    case 's':
      if ((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      for (; *s; s++)
        console_put(*s);
      break;
    case '%':
      console_put('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      console_put('%');
      console_put(c);
      break;
    }
  }
  va_end(ap);

  if (locking)
    release(&pr.lock);
}

void panic(char *s)
{
  pr.locking = 0;
  printf("panic: ");
  printf(s);
  printf("\n");
  panicked = 1; // freeze uart output from other CPUs
  for (;;)
    ;
}

void printf_init()
{
  initlock(&pr.lock);
  pr.locking = 1;
}
