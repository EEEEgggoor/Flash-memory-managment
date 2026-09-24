#ifndef INODE_H
#define INODE_H
#include "extent.h"
#include "flash_ops/flash_params.h"


#define MAX_FILES 128
#define INODE_TABLE_SLOTS 4
#define INODE_TABLE_HEADER_BYTES SECTOR_SIZE // целый сектор под generation + служебное — с запасом
#define INODE_TABLE_DATA_BYTES (MAX_FILES * sizeof(file_inode_t)) // 14336
#define INODE_TABLE_TOTAL_SECTORS ((INODE_TABLE_HEADER_BYTES + INODE_TABLE_DATA_BYTES + SECTOR_SIZE - 1) / SECTOR_SIZE) // 5 секторов



typedef struct
{
    char name[32];
    uint32_t size;
    extent_t extents[MAX_EXTENTS];
    uint32_t ext_count;
    uint32_t ctime;
    uint8_t flags;

} file_inode_t;






#endif //INODE_H