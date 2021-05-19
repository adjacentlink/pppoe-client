#ifndef COMMON_LOGGER_H
#define COMMON_LOGGER_H

#include<stdio.h>
#include<stdarg.h>

#ifndef LOG_ERR
#define LOG_ERR 1
#endif

#ifndef LOG_INFO
#define LOG_INFO 2
#endif

#ifndef LOG_PKT
#define LOG_PKT 3
#endif

#ifndef DEBUG
#define DEBUG   4
#endif

extern FILE * LoggerFp;

extern int verbose_level;

void logger(int lvl, const char * func, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

#define LOGGER(lvl, fmt, ...) \
       logger(lvl, __func__, fmt, ##__VA_ARGS__);

#endif
