#ifndef _DEBUG_H_
#define _DEBUG_H_

#define DB_FATAL 0
#define DB_WARNING 1
#define DB_DEBUG 2
#define DB_EVERYTHING 3

void d_log(int, char *);

#endif // _DEBUG_H_