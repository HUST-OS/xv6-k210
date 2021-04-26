#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "xv6-user/user.h"

#include <stdarg.h>

#define BUF_SIZE 128

static char digits[] = "0123456789ABCDEF";
static char *_outbuf = NULL;
static int _idx = 0;

void
putc(int ch, int fd)
{
  write(fd, &ch, 1);
}

void
putchar(int ch)
{
  write(1, &ch, 1);
}

static void
fflush(int fd)
{
  if (_idx != 0) {
    write(fd, _outbuf, _idx);
    _idx = 0;
  }
}

static void
outchar(int fd, char c)
{
  if (_outbuf == NULL) {
    _outbuf = (char *)malloc(BUF_SIZE);
    _idx = 0;
  }
  _outbuf[_idx++] = c;
  if (_idx == BUF_SIZE) {
    fflush(fd);
  }
}

static void
printint(int fd, int xx, int base, int sgn)
{
  char buf[16];
  int i, neg;
  uint x;

  neg = 0;
  if(sgn && xx < 0){
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  do{
    buf[i++] = digits[x % base];
  }while((x /= base) != 0);
  if(neg)
    buf[i++] = '-';

  while(--i >= 0)
    outchar(fd, buf[i]);
}

static void
printptr(int fd, uint64 x) {
  int i;
  outchar(fd, '0');
  outchar(fd, 'x');
  for (i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
    outchar(fd, digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print to the given fd. Only understands %d, %x, %p, %s.
void
vprintf(int fd, const char *fmt, va_list ap)
{
  char *s;
  int c, i, state;

  state = 0;
  for(i = 0; fmt[i]; i++){
    c = fmt[i] & 0xff;
    if(state == 0){
      if(c == '%'){
        state = '%';
      } else {
        outchar(fd, c);
      }
    } else if(state == '%'){
      if(c == 'd'){
        printint(fd, va_arg(ap, int), 10, 1);
      } else if(c == 'l') {
        printint(fd, va_arg(ap, uint64), 10, 0);
      } else if(c == 'x') {
        printint(fd, va_arg(ap, int), 16, 0);
      } else if(c == 'p') {
        printptr(fd, va_arg(ap, uint64));
      } else if(c == 's'){
        s = va_arg(ap, char*);
        if(s == 0)
          s = "(null)";
        while(*s != 0){
          outchar(fd, *s);
          s++;
        }
      } else if(c == 'c'){
        outchar(fd, va_arg(ap, uint));
      } else if(c == '%'){
        outchar(fd, c);
      } else {
        // Unknown % sequence.  Print it to draw attention.
        outchar(fd, '%');
        outchar(fd, c);
      }
      state = 0;
    }
  }
  fflush(fd);
}

void
fprintf(int fd, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(fd, fmt, ap);
}

void
printf(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(1, fmt, ap);
}
