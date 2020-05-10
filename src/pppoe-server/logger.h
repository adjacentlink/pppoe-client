#ifndef LOGGER_H
#define LOGGER_H

#include<stdio.h>
#include<stdarg.h>

#ifndef LOG_ERR
#define LOG_ERR 1
#endif

#ifndef DEBUG
#define DEBUG   1
#endif

extern FILE * LoggerFp;

#define LOGGER(lvl, fmt, ...) \
        do {  \
           if (DEBUG) \
             fprintf(LoggerFp ? LoggerFp : stdout, fmt, ##__VA_ARGS__); \
            } while (0)

#endif
