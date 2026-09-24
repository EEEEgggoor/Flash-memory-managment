#ifndef FLASH_PARAMS_H
#define FLASH_PARAMS_H

#include <stdint.h>

/* ==================== Параметры физического чипа ====================
 * Текущий чип на плате — Winbond W25Q64FV, 8 Мбайт (см. также DEV_PATH
 * в flash_header.h и DEVICE_NAME в module_kernel/w25q_cdev.c).
 * При смене чипа достаточно поменять эту константу — вся остальная
 * геометрия (число секторов, размер WL/inode таблиц, DATA_START_SECTOR
 * и т.д.) пересчитывается автоматически из неё. */
#define FLASH_CHIP_SIZE_MB         8
 
#define FLASH_CHIP_CAPACITY_BYTES  (FLASH_CHIP_SIZE_MB * 1024 * 1024)

#define SECTOR_SIZE                4096
#define PAGE_SIZE                  256    /* размер страницы для PAGE_PROGRAM */

#define FLASH_TOTAL_SECTORS        (FLASH_CHIP_CAPACITY_BYTES / SECTOR_SIZE)

/* ==================== Wear-leveling таблица ====================
 * На каждый сектор данных приходится 2 uint32: [0] = занято байт в
 * секторе, [1] = счётчик стираний (используется для round-robin
 * выбора наименее изношенного сектора). */
#define WL_ENTRIES_PER_SECTOR      2
#define WL_TABLE_ENTRIES           (FLASH_TOTAL_SECTORS * WL_ENTRIES_PER_SECTOR)
#define WL_TABLE_BYTES             (WL_TABLE_ENTRIES * (int)sizeof(uint32_t))
#define WL_TABLE_SECTORS_PER_SLOT  ((WL_TABLE_BYTES + SECTOR_SIZE - 1) / SECTOR_SIZE)
#define WL_TABLE_SLOTS             4

/* Сколько секторов в начале флеша занято под все 4 generation-слота
   WL-таблицы (сектора 0..META_SECTORS-1). */
#define META_SECTORS               (WL_TABLE_SLOTS * WL_TABLE_SECTORS_PER_SLOT)

#endif //FLASH_PARAMS_H
