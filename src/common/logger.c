

#include"logger.h"
#include<time.h>
#include<sys/time.h>

void logger(int lvl, const char * func, const char *fmt, ...)
{
     if (lvl) { 
         char buff[1024] = {0}; 
         struct tm tm; 
         struct timeval tv; 
         gettimeofday(&tv, NULL); 
         localtime_r(&tv.tv_sec, &tm); 

         const int len = snprintf(buff, sizeof(buff), "[%02u:%02u:%02u.%03lu][%d][%s]:", 
                                  tm.tm_hour,      
                                  tm.tm_min,       
                                  tm.tm_sec,       
                                  tv.tv_usec/1000, 
                                  lvl,              
                                  func);

         va_list arg;
         va_start(arg, fmt);
         vsnprintf(buff + len, sizeof(buff) - len, fmt, arg);
         va_end(arg);

         fprintf(LoggerFp ? LoggerFp : stdout, "%s", buff); 
       } 
}
