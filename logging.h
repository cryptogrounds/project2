#ifndef LOGGING_H
#define LOGGING_H
#include <stdio.h>
#include <string.h>

#ifndef __FILE_NAME__
#define __FILE_NAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

extern int LOG_LEVEL;

#define TRACE	if (LOG_LEVEL > 5) { fprintf(stderr, "TRACE: "
#define DEBUG	if (LOG_LEVEL > 4) { fprintf(stderr, "DEBUG: "
#define INFO	if (LOG_LEVEL > 3) { fprintf(stderr, "INFO: "
#define WARNING	if (LOG_LEVEL > 2) { fprintf(stderr, "WARNING: "
#define ERROR	if (LOG_LEVEL > 1) { fprintf(stderr, "ERROR: "
#define FATAL	if (LOG_LEVEL > 0) { fprintf(stderr, "FATAL: "
#define ENDL	" (%s:%d)\n", __FILE_NAME__, __LINE__); } 1==1
#endif
