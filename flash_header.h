#ifndef FLASH_HEADER_H
#define FLASH_HEADER_H

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <time.h>
#include <stdint.h>

#define SPI_PATH "/dev/spidev3.0"

#define CHECK_FLASH 0x9F

#define WRITE_ENABLE 0x06
#define PAGE_PROGRAM 0x02
#define SECTOR_ERASE 0x20
#define READ_DATA 0x03
#define READ_STATUS 0x05
#define CHIP_ERASE 0xC7



#define META_SECTORS 8


#define MODE_ERASED   0   // сектор был физически стёрт
#define MODE_APPENDED 1   // дозаписали данные без стирания


static int fd;
uint32_t buff_table[4][2048];
uint32_t tables[4] = {0x000000, 0x002000, 0x004000, 0x006000};

#endif