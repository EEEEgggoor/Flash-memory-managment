#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <time.h>

#include "file_system/FS_fops.h"
#include "run_fs_tests.h"


uint8_t r_hex(){ return (uint8_t)(rand() & 0xFF); }


int main(int argc, char *argv[]){

	srand(time(NULL));
	
	flash_init("/dev/W25Q64FV");

	if(!strcmp(argv[1], "--inode")){
		garbage_collection(); 
	}


	if(!strcmp(argv[1], "--info")){
		printf("sizeof(file_inode_t) = %zu\n", sizeof(file_inode_t));
		printf("total inode table size = %zu bytes = %.2f sectors\n",
			sizeof(file_inode_t) * MAX_FILES,
			(double)(sizeof(file_inode_t) * MAX_FILES) / 4096.0);
 
		printf("WL_TABLE_PAGE-------------------------------------------------------------------\n");

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


		printf("\n");

		size_t total_len = SECTOR_SIZE * 38;
		uint8_t *buf = (uint8_t*)malloc(total_len);
		if (!buf) {
			perror("malloc");
			close(fd);
			return 1;
		}

		size_t chunk = BUF_SIZE;
		size_t offset = 0;
		while (offset < total_len) {
			size_t this_len = (total_len - offset < chunk) ? (total_len - offset) : chunk;
			read_data((uint32_t)offset, buf + offset, this_len);
			offset += this_len;
		}
		

		printf("WL_TABLE_SECTOR-------------------------------------------------------------------\n");
		for (int i = 0; i < 8; i++) {
			printf("Sector %2d: ", i);
			for (int j = 0; j < 50; j++) {
				printf("%02X ", buf[i * SECTOR_SIZE + j]);
			}
			printf("\n");
		}

		printf("INODE_TABLE-------------------------------------------------------------------\n");
		for (int i = 8; i < 28; i++) {
			printf("Sector %2d: ", i);
			for (int j = 0; j < 224; j++) {
				printf("%02X ", buf[i * SECTOR_SIZE + j]);
			}
			printf("\n");
		}


		printf("DATA-------------------------------------------------------------------\n");
		for (int i = 28; i < 38; i++) {
			printf("Sector %2d: ", i);
			for (int j = 0; j < 50; j++) {
				printf("%02X ", buf[i * SECTOR_SIZE + j]);
			}
			printf("\n");
		}

		free(buf);
	}
	 
    if(!strcmp(argv[1], "--n_WL_table")){
        create_tables_wear_levering();
		create_inodes_table();
        printf("Tables init\n");
        close(fd);
        return 0;
    }

	if(!strcmp(argv[1], "--reset_flash")){
		chip_erase();

	}

	if(!strcmp(argv[1], "--write")){

		int num = atoi(argv[3]);

		uint8_t buff[num];
		uint8_t buff_flash[num];
		int num_prt;

		for(int i = 0; i < num; i++) { buff[i] = r_hex(); }
		printf("write to flash: ");
		if(num > 50) { num_prt = 15; }
		else{ num_prt = num; }
		for(int i = 0; i < num_prt; i++){ printf(" 0x%02X ", buff[i]); }
		printf(" ...\n");

		write_file(argv[2], buff, num);
		
	}
	if(!strcmp(argv[1], "--read")){
		int num = atoi(argv[3]);
		int num_prt;
		uint8_t buff_flash[num];

		printf("read from flash: ");

		read_file(argv[2], buff_flash, num);

		if(num > 50) { num_prt = 15; }
		else{ num_prt = num; }
		for(int i = 0; i < num_prt; i++){ printf(" 0x%02X ", buff_flash[i]); }
		printf(" ...\n");
	}  
 
	if(!strcmp(argv[1], "--del")){
		int num_prt;

		printf("delete %s ...\n", argv[2]);

		int d = delete_file(argv[2]);
		if(d == 0) { printf(" finish delete %s\n", argv[2]); } else{ printf("error delete %s\n", argv[2]); }
	}

	if(!strcmp(argv[1], "--test")){
		run_fs_tests();
	}

	close(fd);
	return 0;
}