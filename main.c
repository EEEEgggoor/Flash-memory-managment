#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <time.h>

#include "file_system/_init.h"


uint8_t r_hex(){ return (uint8_t)(rand() & 0xFF); }


int main(int argc, char *argv[]){

	srand(time(NULL));
	
	int num = atoi(argv[1]);

	flash_init("/dev/W25Q64FV");

	if(num == -123){
		printf("sizeof(file_inode_t) = %zu\n", sizeof(file_inode_t));
		printf("total inode table size = %zu bytes = %.2f sectors\n",
			sizeof(file_inode_t) * MAX_FILES,
			(double)(sizeof(file_inode_t) * MAX_FILES) / 4096.0);
	}
	
    if(num == -3){
        create_tables_wear_levering();
        printf("Tables initialized\n");
        close(fd);
        return 0;
    }



	if(num > 0){

		uint8_t buff[num];
		uint8_t buff_flash[num];
		int num_prt;

		for(int i = 0; i < num; i++) { buff[i] = r_hex(); }
		printf("write to flash: ");
		if(num > 50) { num_prt = 15; }
		else{ num_prt = num; }
		for(int i = 0; i < num_prt; i++){ printf(" 0x%02X ", buff[i]); }
		printf(" ...\n");

		extent_list_t ext = write_data(buff, num);

		read_extents(&ext, buff_flash, num);

		printf("[ ");
		for(int i = 0; i < ext.count; i++){
			printf("extents: 0x%06X:%u ", ext.extents[i].addr, ext.extents[i].len);
		}
		printf("]\n");


		printf("read from flash: ");
			for(int i = 0; i < num_prt; i++){ printf(" 0x%02X ", buff_flash[i]); }
		printf(" ...\n");




		// chip_erase();
		// create_tables();


		for(int k = 0; k < 4; k ++){
			for(int i = 0; i < 2048; i++){
				buff_table[k][i] = read_uint32(tables[k] + 4*i);
			}
		}
		
		
		uint32_t max_gen = 0;
		uint32_t index_max_gen = 0;
		for(int i = 0; i < 4; i++){
			if(max_gen < buff_table[i][0]) { max_gen = buff_table[i][0]; index_max_gen = i; }
		}




		printf("---------------------------------------------------------------------\n");


		for(int i = 0; i < 1; i++){
			printf("GEN %d: ", max_gen);
			for(int j = 0; j < 16; j++){
				if(i==0 && j != 0 && j < 16) {printf(" MD "); }
			}
		}
		printf("\n");

		for(int i = 0; i < 32; i++){
			printf("PAGE %d: ", i);
			for(int j = 0; j < 64; j++){
				if((i==0 && j != 0 && j < 16) || (i == 0 && j == 0)) {  }
				else { printf(" %d ", buff_table[index_max_gen][i*64 + j]); }
			}
			printf("\n");
		}


		
		free(ext.extents);
	}
	if(num == -2){
		size_t total_len = SECTOR_SIZE * 20; // 81920 байт
		uint8_t *buf = (uint8_t*)malloc(total_len);
		if (!buf) {
			perror("malloc");
			close(fd);
			return 1;
		}

		for (int i = 0; i < 20; i++) {
			uint32_t addr = (uint32_t)i * SECTOR_SIZE;
			read_data(addr, buf + (size_t)i * SECTOR_SIZE, SECTOR_SIZE);
			printf("Сектор %2d (addr 0x%06X) прочитан\n", i, addr);
		}

		/* Пример: вывод первых 16 байт каждого сектора для проверки */
		for (int i = 0; i < 20; i++) {
			printf("Sector %2d: ", i);
			for (int j = 0; j < 16; j++) {
				printf("%02X ", buf[i * SECTOR_SIZE + j]);
			}
			printf("\n");
		}		
	}
	
	close(fd);
	return 0;
}
