/**
 * syscalls.c -- minimal newlib stubs for a bare-metal target.
 * lwIP uses static pools (no malloc); _sbrk is provided only so the C library
 * links cleanly. There is no filesystem or console: I/O calls are no-ops.
 */
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* Provided by the linker script */
extern char end;          /* start of heap */
extern char _estack;      /* top of stack  */

void *_sbrk(ptrdiff_t incr)
{
  static char *heap = &end;
  char *prev = heap;
  /* Leave room for the stack growing down from _estack. */
  if (heap + incr > (char *)&_estack) {
    errno = ENOMEM;
    return (void *)-1;
  }
  heap += incr;
  return prev;
}

int _write(int fd, const char *buf, int len) { (void)fd; (void)buf; return len; }
int _read(int fd, char *buf, int len)        { (void)fd; (void)buf; (void)len; return 0; }
int _close(int fd)                           { (void)fd; return -1; }
int _lseek(int fd, int off, int dir)         { (void)fd; (void)off; (void)dir; return 0; }
int _fstat(int fd, struct stat *st)          { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd)                          { (void)fd; return 1; }
int _getpid(void)                            { return 1; }
int _kill(int pid, int sig)                  { (void)pid; (void)sig; errno = EINVAL; return -1; }
void _exit(int code)                         { (void)code; while (1) {} }
