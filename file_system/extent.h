#ifndef EXTENT_H
#define EXTENT_H


#include <stdint.h>
#define MAX_EXTENTS 32

typedef struct {
    uint32_t addr;
    uint32_t len;
} extent_t;

typedef struct {
    extent_t *extents;
    uint8_t count;
} extent_list_t;

#endif