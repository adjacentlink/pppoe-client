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

void logger(int lvl, const char * func, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

#define LOGGER_F(lvl, fmt, ...) \
        do { \
           if (lvl) { \
             char _buff[1024] = {0}; \
             struct tm _tm; \
             struct timeval _tv; \
             gettimeofday(&_tv, NULL); \
             localtime_r(&_tv.tv_sec, &_tm); \
             const int _len = snprintf(_buff, sizeof(_buff), "[%02u:%02u:%02u.%03lu][%d][%s]:", \
                                       _tm.tm_hour,      \
                                       _tm.tm_min,       \
                                       _tm.tm_sec,       \
                                       _tv.tv_usec/1000, \
                                       lvl,              \
                                       __func__);        \
             snprintf(_buff + _len, sizeof(_buff) - _len, fmt, ##__VA_ARGS__); \
             fprintf(LoggerFp ? LoggerFp : stdout, "%s", _buff); \
           } \
         } while (0)


#define LOGGER(lvl, fmt, ...) \
       logger(lvl, __func__, fmt, ##__VA_ARGS__);


#endif
