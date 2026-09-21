#ifndef FLASH_OPER_H
#define FLASH_OPER_H

#include "flash_header.h"

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

static inline int flash_init(const char* path){

    fd = open(path, O_RDWR);
    if (fd < 0) { perror("open w25q device"); return -1; }

    uint8_t tx[1] = {CHECK_FLASH};
    if (write(fd, tx, 1) != 1) { perror("write CHECK_FLASH"); return -1; }

    uint8_t id[3] = {0};
    if (read(fd, id, sizeof(id)) != (ssize_t)sizeof(id)) {
        perror("read CHECK_FLASH result");
        return -1;
    }

    printf("Manufacturer ID: 0x%02X, Device ID: 0x%02X%02X\n",
           id[0], id[1], id[2]);

    return 0;
}

static inline int read_status(void){

    uint8_t tx[1] = {READ_STATUS};
    if (write(fd, tx, 1) != 1) { perror("write READ_STATUS"); return -1; }

    uint8_t status = 0;
    if (read(fd, &status, 1) != 1) { perror("read READ_STATUS result"); return -1; }

    return status;
}

static inline void write_enable(void){
    
    uint8_t tx[1] = {WRITE_ENABLE};
    if (write(fd, tx, 1) != 1) { perror("write WRITE_ENABLE"); }
}

static inline void wait_busy(void){
    int status;
    do {
        status = read_status();
        if (status < 0) return; /* ошибка чтения — выходим, чтобы не зациклиться */
        usleep(1000);
    } while (status & 0x01);
}

static inline int sector_erase(uint32_t addr){

    uint8_t tx[4] = {
        SECTOR_ERASE,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
    };

    if (write(fd, tx, sizeof(tx)) != (ssize_t)sizeof(tx)) {
        perror("write SECTOR_ERASE");
        return -1;
    }

    wait_busy();
    return 0;
}

static inline void page_program(uint32_t addr, uint8_t *data, size_t len){

    size_t buf_len = 4 + len;
    uint8_t *tx = (uint8_t*)malloc(buf_len);
    if (!tx) { perror("malloc"); return; }

    tx[0] = PAGE_PROGRAM;
    tx[1] = (addr >> 16) & 0xFF;
    tx[2] = (addr >> 8) & 0xFF;
    tx[3] = addr & 0xFF;
    memcpy(tx + 4, data, len);

    /* write_enable() не нужен — драйвер делает это сам */
    if (write(fd, tx, buf_len) != (ssize_t)buf_len) {
        perror("write PAGE_PROGRAM");
    }

    wait_busy();
    free(tx);
}

static inline void read_data(uint32_t addr, uint8_t* buf, size_t len){

    struct w25q_read_req req = {
        .addr = addr,
        .len  = len,
    };

    if (ioctl(fd, W25Q_READ_DATA, &req) < 0) {
        perror("ioctl W25Q_READ_DATA");
        return;
    }

    if (read(fd, buf, len) != (ssize_t)len) {
        perror("read READ_DATA result");
    }
}

static inline void chip_erase(void){
    uint8_t tx[1] = {CHIP_ERASE};

    if (write(fd, tx, 1) != 1) {
        perror("write CHIP_ERASE");
        return;
    }

    wait_busy();
}

#endif
