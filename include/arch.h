#include "stdint.h"

#if INTPTR_MAX == INT64_MAX
    #define ARCH64
#elif INTPTR_MAX == INT32_MAX
    #define ARCH32
#endif
