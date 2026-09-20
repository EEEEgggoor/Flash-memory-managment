#ifndef INODE_H
#define INODE_H
#include "extent.h"


#define MAX_FILES 128
#define INODE_TABLE_HEADER_BYTES 4096 // целый сектор под generation + служебное — с запасом
#define INODE_TABLE_DATA_BYTES (MAX_FILES * sizeof(file_inode_t)) // 8192
#define INODE_TABLE_TOTAL_SECTORS ((INODE_TABLE_HEADER_BYTES + INODE_TABLE_DATA_BYTES) / 4096) // 3 сектора

typedef struct
{
    uint32_t generation;
    char name[32];
    uint32_t size;
    extent_list_t ext;
    uint32_t ctime;
    uint8_t flags;
} file_inode_t;



#endif //INODE_H