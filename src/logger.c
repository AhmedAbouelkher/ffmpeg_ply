#include "logger.h"
#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

void log_message(const char *fmt, ...) {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  struct tm *tm_info = localtime(&tv.tv_sec);

  char time_buf[30];
  strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

  printf("[%s.%03d] ", time_buf, (int)(tv.tv_usec / 1000));

  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

void log_message_ln(const char *fmt, ...) {
  log_message(fmt);
  printf("\n");
}