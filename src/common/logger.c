

#include"logger.h"
#include<time.h>
#include<sys/time.h>

int verbose_level = 3;

void logger(int lvl, const char * func, const char *fmt, ...)
{
     if (lvl <= verbose_level) { 
         char buff[1024] = {0}; 
         struct tm tm; 
         struct timeval tv; 
         gettimeofday(&tv, NULL); 
         localtime_r(&tv.tv_sec, &tm); 

         int len = snprintf(buff, sizeof(buff), "[%02u:%02u:%02u.%03lu][%06d][%s]:", 
                                  tm.tm_hour,      
                                  tm.tm_min,       
                                  tm.tm_sec,       
                                  tv.tv_usec, 
                                  lvl,              
                                  func);

         va_list arg;
         va_start(arg, fmt);
         len += vsnprintf(buff + len, sizeof(buff) - len - 1, fmt, arg);
         va_end(arg);

#undef PRINT_LOG_STDERR

         if(buff[len-1] != '\n') {
#ifdef PRINT_LOG_STDERR
           fprintf(stderr, "%s\n", buff);
#else
           fprintf(LoggerFp ? LoggerFp : stdout, "%s\n", buff); 
#endif
          }
         else {
#ifdef PRINT_LOG_STDERR
           fprintf(stderr, "%s", buff);
#else
           fprintf(LoggerFp ? LoggerFp : stdout, "%s", buff); 
#endif
          }
       } 
}
