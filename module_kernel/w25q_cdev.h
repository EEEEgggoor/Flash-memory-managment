#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/of.h>

//SPI
#include <linux/spi/spi.h>

#include "w25q_ioctl.h"


#define DEVICE_NAME "W25Q64FV"
#define BUF_SIZE 4096

#define CHECK_FLASH 0x9F

#define WRITE_ENABLE 0x06
#define PAGE_PROGRAM 0x02
#define SECTOR_ERASE 0x20
#define READ_DATA 0x03
#define READ_STATUS 0x05
#define CHIP_ERASE 0xC7
#define WRITE_STATUS_REG 0x01


#define W25Q_EXPECTED_MFR_ID   0xEF
#define W25Q_EXPECTED_MEM_TYPE 0x40


struct w25q {
    struct spi_device *spi;
    dev_t dev_num;
    struct cdev flash_cdev;
    struct class *flash_class;
    struct device *flash_device;
    char *kernel_buffer;
    size_t data_size;
    struct mutex lock;
    u32 pending_addr;
    u32 pending_len;

    bool result_ready;
};


struct class *w25q_class;