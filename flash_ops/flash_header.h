#ifndef FLASH_HEADER_H
#define FLASH_HEADER_H

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <stdint.h>

#include "module_kernel/w25q_ioctl.h"
#include "flash_ops/flash_params.h"
#include "file_system/indode.h"

#define DEV_PATH "/dev/W25Q64FV"

#define CHECK_FLASH 0x9F

#define WRITE_ENABLE 0x06
#define PAGE_PROGRAM 0x02
#define SECTOR_ERASE 0x20
#define READ_DATA 0x03
#define READ_STATUS 0x05
#define CHIP_ERASE 0xC7

/* BUF_SIZE — лимит одной ioctl+read транзакции в модуле ядра
   (module_kernel/w25q_cdev.c: BUF_SIZE). Совпадает числом с SECTOR_SIZE,
   но это отдельная величина — контракт драйвера, а не геометрия флеша,
   поэтому она не выводится из SECTOR_SIZE и меняется отдельно, если
   когда-нибудь изменится BUF_SIZE в модуле ядра. */
#define BUF_SIZE 4096


#define MODE_ERASED   0   // сектор был физически стёрт
#define MODE_APPENDED 1   // дозаписали данные без стирания


static int fd;

/* [slot][entry], slot = 0..WL_TABLE_SLOTS-1, entry = 0..WL_TABLE_ENTRIES-1 */
uint32_t buff_table[WL_TABLE_SLOTS][WL_TABLE_ENTRIES];

/* Адреса начала каждого из 4 generation-слотов WL-таблицы.
   Каждый слот занимает WL_TABLE_SECTORS_PER_SLOT секторов подряд. */
uint32_t tables[WL_TABLE_SLOTS] = {
    0 * WL_TABLE_SECTORS_PER_SLOT * SECTOR_SIZE,
    1 * WL_TABLE_SECTORS_PER_SLOT * SECTOR_SIZE,
    2 * WL_TABLE_SECTORS_PER_SLOT * SECTOR_SIZE,
    3 * WL_TABLE_SECTORS_PER_SLOT * SECTOR_SIZE,
};

/* Адреса начала каждого из 4 generation-слотов inode-таблицы.
   Идут сразу после области WL-таблицы (после META_SECTORS секторов). */
uint32_t inodes[INODE_TABLE_SLOTS] = {
    META_SECTORS * SECTOR_SIZE + 0 * INODE_TABLE_TOTAL_SECTORS * SECTOR_SIZE,
    META_SECTORS * SECTOR_SIZE + 1 * INODE_TABLE_TOTAL_SECTORS * SECTOR_SIZE,
    META_SECTORS * SECTOR_SIZE + 2 * INODE_TABLE_TOTAL_SECTORS * SECTOR_SIZE,
    META_SECTORS * SECTOR_SIZE + 3 * INODE_TABLE_TOTAL_SECTORS * SECTOR_SIZE,
};

/* Начало области данных — сразу после всех 4 слотов inode-таблицы. */
#define DATA_START_SECTOR (META_SECTORS + INODE_TABLE_SLOTS * INODE_TABLE_TOTAL_SECTORS)

/* Раньше было dead_sectors[1024] — вдвое меньше реальной ёмкости чипа
   (2048 секторов на 8 МБ), из-за чего garbage_collection и allocator
   физически не могли видеть/использовать вторую половину флеша.
   Теперь размер выводится из фактической ёмкости чипа. */
uint32_t dead_sectors[FLASH_TOTAL_SECTORS] = {0};


static inline uint32_t NUMBER_SECTOR(uint32_t addr){ return addr / SECTOR_SIZE; }

#endif
