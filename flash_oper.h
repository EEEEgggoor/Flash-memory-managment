#ifndef FLASH_OPER_H
#define FLASH_OPER_H


#include "flash_header.h"

static inline int flash_init(const char* path, uint8_t _mode, uint8_t _bits, uint32_t _speed){

        fd = open(path, O_RDWR);
        if (fd < 0) { perror("open spi"); return -1; }

        uint8_t mode = _mode;
        uint8_t bits = _bits;
        uint32_t speed = _speed;

        if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) { perror("SPI_IOC_WR_MODE"); return -1; }
        if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) { perror("SPI_IOC_WR_BITS_PER_WORD"); return -1; }
        if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) { perror("SPI_IOC_WR_MAX_SPEED_HZ"); return -1; }

        uint8_t tx[4] = {CHECK_FLASH, 0x00, 0x00, 0x00};
        uint8_t rx[4] = {0};
        struct spi_ioc_transfer tr = {
                .tx_buf = (unsigned long)tx,
                .rx_buf = (unsigned long)rx,
                .len = 4,
                .speed_hz = speed,
                .bits_per_word = bits,
        };

        int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
        if (ret < 0) { perror("SPI_IOC_MESSAGE"); return -1; }

        printf("Manufacturer ID: 0x%02X, Device ID: 0x%02X%02X\n", rx[1], rx[2], rx[3]);
	
	return 0;
}

static inline int spi_transfer(uint8_t *tx, uint8_t *rx, size_t len){

	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = len,
		.speed_hz = 20000000,
		.delay_usecs = 0,
        .bits_per_word = 8,
	};

	
	return ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
}

static inline int read_status(){

	uint8_t tx[2] = {READ_STATUS, 0};
	uint8_t rx[2] ={0};
	spi_transfer(tx, rx, 2);
	return rx[1];
}


static inline void write_enable(){
	uint8_t tx[1] = {WRITE_ENABLE};
	uint8_t rx[1] = {0};
	spi_transfer(tx, rx, 1);
}

static inline void wait_busy(){

	while(read_status() & 0x01){
		usleep(1000);
	}
}

static inline int sector_erase(uint32_t addr){
	
	
	uint8_t tx[4] = {SECTOR_ERASE, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF};
	uint8_t rx[4] = {0};

	write_enable();
	spi_transfer(tx, rx, 4);
	wait_busy();

	return 0;
}

static inline void page_program(uint32_t addr, uint8_t *data, size_t len){
	
	size_t len_tx_rx = 4 + len;

	uint8_t *tx = (uint8_t*)malloc(len_tx_rx);
	uint8_t *rx = (uint8_t*)malloc(len_tx_rx);

	tx[0] = PAGE_PROGRAM;
	tx[1] = (addr >> 16) & 0xFF;
	tx[2] = (addr >> 8) & 0xFF;
	tx[3] = (addr) & 0xFF;
	
	memcpy(4 + tx, data, len);
	
	write_enable();
	spi_transfer(tx, rx, len_tx_rx);
	wait_busy();

	free(tx);
	free(rx);
	
}


static inline void read_data(uint32_t addr, uint8_t* buf, size_t len){
	
	size_t len_tx_rx = 4 + len;
	uint8_t *tx = (uint8_t*)malloc(len_tx_rx);
	uint8_t *rx = (uint8_t*)malloc(len_tx_rx);

	memset(tx, 0, len_tx_rx);

	tx[0] = READ_DATA;
	tx[1] = (addr >> 16) & 0xFF;
	tx[2] = (addr >> 8) & 0xFF;
	tx[3] = (addr) & 0xFF;

	write_enable();
	spi_transfer(tx, rx, len_tx_rx);
	
	memcpy(buf, 4 + rx, len);
	
	free(tx);
	free(rx);
}

static inline void chip_erase(void){
    uint8_t tx[1] = {CHIP_ERASE};
    uint8_t rx[1] = {0};

    write_enable();
    spi_transfer(tx, rx, 1);
    wait_busy();
}

#endif