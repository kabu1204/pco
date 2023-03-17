#ifndef PCO_INFO_H
#define PCO_INFO_H
#include "stdio.h"

#define INFO(fmt, ...) \
            do { fprintf(stderr,"[INFO] " fmt, __VA_ARGS__); } while (0)

#endif