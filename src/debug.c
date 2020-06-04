#include <stdio.h>
#include <string.h>

#include "debug.h"

int DB_LEVEL = DB_DEBUG;

void d_log(int level, char *msg) {
    if(level <= DB_LEVEL) {
        if(level == DB_DEBUG || level == DB_EVERYTHING) printf("%s", msg);
        else fprintf(stderr, "%s", msg);
    }
}