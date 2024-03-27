/*
 * Console input and output, to the uart.
 * Implements special input characters:
 *   newline -- end of line
 *   control-p -- print process list
 */

#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

#define BACKSPACE 0x100
#define INPUT_BUF_SIZE 1024
#define C(x)  ((x)-'@')  // Control-x

struct console {
  struct spinlock lock;
  char buf[INPUT_BUF_SIZE];
  unsigned int read_index;
  unsigned int write_index;
  unsigned int edit_index;
};

static struct console console;

/*
 * For printf() and user echoes.
 * Doesn't block.
 */
void console_put(int c)
{
  if (c == BACKSPACE) {
    /* Overwrite with a space. */
    uartputc_sync('\b'); uartputc_sync(' '); uartputc_sync('\b');
  } else {
    uartputc_sync(c);
  }
}

/*
 * For user write()s.
 * Blocks, so can't call from interrupts.
 * Returns the number of bytes written.
 */
static int console_write(unsigned long src, int n)
{
  char c;
  int i;

  for (i = 0; i < n; i++) {
    if (copy_from_user(myproc()->pagetable, &c, src + i, sizeof(char)))
      break;

    uartputc(c);
  }

  return i;
}

/*
 * For user read()s from the console.
 * Returns number of bytes read.
 */
static int console_read(unsigned long dst, int n)
{
  unsigned int target = n;
  int c;

  acquire(&console.lock);

  while (n > 0) {
    /* Wait until interrupt handler has put some input into console buffer. */
    while (console.read_index == console.write_index) {
      if (killed(myproc())) {
        release(&console.lock);
        return -1;
      }

      sleep(&console.read_index, &console.lock);
    }

    c = console.buf[console.read_index++ % INPUT_BUF_SIZE];

    if (copy_to_user(myproc()->pagetable, dst, (void *)&c, 1) == -1)
      break;

    dst++;
    --n;

    if (c == '\n')
      break;
  }

  release(&console.lock);

  return target - n;
}

/*
 * The console input interrupt handler.
 * uartintr() calls this for input characters.
 * Do backspace processing, append to console.buf,
 * wake up console_read() if a whole line has arrived.
 */
void console_handle_irq(int c)
{
  acquire(&console.lock);

  switch(c) {
  case C('P'):
    /* Print process list. */
    procdump();
    break;
  case '\x7f':
  /* Backspace */
    if (console.edit_index != console.write_index) {
      console.edit_index--;
      console_put(BACKSPACE);
    }
    break;
  default:
    if (c != 0 && console.edit_index-console.read_index < INPUT_BUF_SIZE) {
      c = (c == '\r') ? '\n' : c;

      /* echo back to the user. */
      console_put(c);

      /* store for consumption later by console_read(). */
      console.buf[console.edit_index++ % INPUT_BUF_SIZE] = c;

      if(c == '\n' || console.edit_index-console.read_index == INPUT_BUF_SIZE){
        /* Enter pressed, wakeup console_read */
        console.write_index = console.edit_index;
        wakeup(&console.read_index);
      }
    }
    break;
  }
  
  release(&console.lock);
}

void console_init()
{
  initlock(&console.lock);

  uart_init();

  /* Connect read and write system calls */
  devsw[CONSOLE].read = console_read;
  devsw[CONSOLE].write = console_write;
}
