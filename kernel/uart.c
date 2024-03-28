/*
 * Driver for 16550a UART.
*/

#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "defs.h"

#define Reg(reg) ((volatile unsigned char *)(UART0 + reg))
#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

#define RECEIVE_HOLDING_REG 0
#define TRANSMIT_HOLDING_REG 0
#define INT_ENABLE_REG 1
#define RX_ENABLE (1<<0)
#define TX_ENABLE (1<<1)
#define FIFO_CTRL_REG 2
#define FIFO_ENABLE (1<<0)
#define FIFO_CLEAR (3<<1)
#define LINE_CONTROL_REG 3
#define EIGHT_BITS (3<<0)
#define BAUD_LATCH (1<<7)
#define LINE_STATUS_REG 5
#define TX_IDLE (1<<5)
#define DISABLE_INTERRUPTS  0x00
#define UART_TX_BUF_SIZE 32

struct uart_tx_buffer {
  struct spinlock lock;
  char buf[UART_TX_BUF_SIZE];
  unsigned long write_index; /* Indicates next write to tx_buf */
  unsigned long read_index; /* Indicates next read from tx_buf */
};

static struct uart_tx_buffer tx_buffer;

extern volatile int panicked;

void uart_init()
{
  WriteReg(INT_ENABLE_REG, DISABLE_INTERRUPTS);

  /* Set baud rate to 38.4K. */
  WriteReg(LINE_CONTROL_REG, BAUD_LATCH);
  WriteReg(0, 0x03);
  WriteReg(1, 0x00);

  /* leave set-baud mode and set word length to 8 bits, no parity. */
  WriteReg(LINE_CONTROL_REG, EIGHT_BITS);

  WriteReg(FIFO_CTRL_REG, FIFO_ENABLE | FIFO_CLEAR);
  WriteReg(INT_ENABLE_REG, TX_ENABLE | RX_ENABLE);

  initlock(&tx_buffer.lock);
}

/* If the UART is idle, and a character is waiting in the transmit buffer, send it.
 * Caller must hold lock.
*/
static void uart_start()
{
  while (tx_buffer.write_index != tx_buffer.read_index &&
         (ReadReg(LINE_STATUS_REG) & TX_IDLE) != 0) {
    /* maybe uartputc() is waiting for space in the buffer. */
    wakeup(&tx_buffer.read_index);
    
    WriteReg(TRANSMIT_HOLDING_REG, tx_buffer.buf[tx_buffer.read_index++ % UART_TX_BUF_SIZE]);
  }
}

/* Add a character to the output buffer and tell the
 * UART to start sending if it isn't already.
 * blocks if the output buffer is full. (for bottom halves)
 */
void uart_put(int c)
{
  acquire(&tx_buffer.lock);

  if (panicked)
    for (;;);

  while (tx_buffer.write_index == tx_buffer.read_index + UART_TX_BUF_SIZE) {
    /* Buffer is full */
    sleep(&tx_buffer.read_index, &tx_buffer.lock);
  }

  tx_buffer.buf[tx_buffer.write_index++ % UART_TX_BUF_SIZE] = c;
  uart_start();
  release(&tx_buffer.lock);
}


/* uart_put() but without sleeping (for top-halves)
 */
void uart_put_sync(int c)
{
  push_off();

  if (panicked)
    for (;;);

  while ((ReadReg(LINE_STATUS_REG) & TX_IDLE) == 0);

  WriteReg(TRANSMIT_HOLDING_REG, c);

  pop_off();
}

/* Read one input character from the UART.
 * Return -1 if none is waiting.
*/
int uart_get()
{
  if (ReadReg(LINE_STATUS_REG) & 0x01)
    return ReadReg(RECEIVE_HOLDING_REG);

  return -1;
}

/* Raised because input has arrived, or the uart is ready for more output, or
 * both. Reads and processes incoming characters. Called from devintr().
*/
void handle_uart_irq()
{
  int c;

  while (1) {
    c = uart_get();
    if (c == -1)
      break;
    console_handle_irq(c);
  }

  /* Send buffered characters. */
  acquire(&tx_buffer.lock);
  uart_start();
  release(&tx_buffer.lock);
}
