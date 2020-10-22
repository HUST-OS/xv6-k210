//
// formatted console output -- printf, panic.
//

#include <stdarg.h>
#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"
#include "sbi.h"

volatile int panicked = 0;

static char digits[] = "0123456789abcdef";

// lock to avoid interleaving concurrent printf's.
static struct {
  struct spinlock lock;
  int locking;
} pr;

void printstring(const char* s) {
    while (*s)
    {
        sbi_console_putchar(*s++);
    }
}

static void
printint(int xx, int base, int sign)
{
  char buf[16];
  int i;
  uint x;

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    sbi_console_putchar(buf[i]);
}


static void
printptr(uint64 x)
{
  int i;
  sbi_console_putchar('0');
  sbi_console_putchar('x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    sbi_console_putchar(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print to the console. only understands %d, %x, %p, %s.
void
printf(char *fmt, ...)
{
  va_list ap;
  int i, c;
  int locking;
  char *s;

  locking = pr.locking;
  if(locking)
    acquire(&pr.lock);
  
  if (fmt == 0)
    panic("null fmt");

  va_start(ap, fmt);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      sbi_console_putchar(c);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(va_arg(ap, int), 10, 1);
      break;
    case 'x':
      printint(va_arg(ap, int), 16, 1);
      break;
    case 'p':
      printptr(va_arg(ap, uint64));
      break;
    case 's':
      if((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      for(; *s; s++)
        sbi_console_putchar(*s);
      break;
    case '%':
      sbi_console_putchar('%');
      break;
    default:
      // Print unknown % sequence to draw attention.
      sbi_console_putchar('%');
      sbi_console_putchar(c);
      break;
    }
  }
  if(locking)
    release(&pr.lock);
}

void
panic(char *s)
{
  printf("panic: ");
  printf(s);
  printf("\n");
  panicked = 1; // freeze uart output from other CPUs
  for(;;)
    ;
}

void
printfinit(void)
{
  initlock(&pr.lock, "pr");
  pr.locking = 1;
}
