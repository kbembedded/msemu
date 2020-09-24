#include "strndup.h"

#if defined(_MSC_VER)

#include <malloc.h>
#include <math.h>
#include <string.h>

char* strndup(const char* s, size_t n)
{
    char* dup = (char*)malloc(n+1);
    memcpy(dup, s, n);
    dup[n] = 0;
    
    return dup;
}

#endif //defined(_MSC_VER)