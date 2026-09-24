#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <time.h>

#include "file_system/load_export_f.h"
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
			(double)(sizeof(file_inode_t) * MAX_FILES) / SECTOR_SIZE);
 
		printf("WL_TABLE_PAGE-------------------------------------------------------------------\n");

		for(int k = 0; k < WL_TABLE_SLOTS; k ++){
			for(int i = 0; i < WL_TABLE_ENTRIES; i++){
				buff_table[k][i] = read_uint32(tables[k] + 4*i);
			}
		}

		uint32_t max_gen = 0;
		uint32_t index_max_gen = 0;
		for(int i = 0; i < WL_TABLE_SLOTS; i++){
			if(max_gen < buff_table[i][0]) { max_gen = buff_table[i][0]; index_max_gen = i; }
		}

		for(int i = 0; i < 1; i++){
			printf("GEN %d: ", max_gen);
			for(int j = 0; j < 16; j++){
				if(i==0 && j != 0 && j < 16) {printf(" MD "); }
			}
		}
		printf("\n");

		/* WL_TABLE_ENTRIES записей выводим построчно по WL_DISPLAY_COLS штук */
		{
			const int WL_DISPLAY_COLS = 64;
			int wl_display_rows = WL_TABLE_ENTRIES / WL_DISPLAY_COLS;
			for(int i = 0; i < wl_display_rows; i++){
				printf("PAGE %d: ", i);
				for(int j = 0; j < WL_DISPLAY_COLS; j++){
					if((i==0 && j != 0 && j < 16) || (i == 0 && j == 0)) {  }
					else { printf(" %d ", buff_table[index_max_gen][i*WL_DISPLAY_COLS + j]); }
				}
				printf("\n");
			}
		}


		printf("\n");

		/* Показываем всю мета-область (WL + inode таблицы) плюс небольшой
		   кусок начала области данных для превью. */
		size_t preview_data_sectors = 10;
		size_t total_len = (size_t)(DATA_START_SECTOR + preview_data_sectors) * SECTOR_SIZE;
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
		for (int i = 0; i < META_SECTORS; i++) {
			printf("Sector %2d: ", i);
			for (int j = 0; j < 50; j++) {
				printf("%02X ", buf[i * SECTOR_SIZE + j]);
			}
			printf("\n");
		}

		printf("INODE_TABLE-------------------------------------------------------------------\n");
		for (int i = META_SECTORS; i < DATA_START_SECTOR; i++) {
			printf("Sector %2d: ", i);
			for (int j = 0; j < 224; j++) {
				printf("%02X ", buf[i * SECTOR_SIZE + j]);
			}
			printf("\n");
		}


		printf("DATA-------------------------------------------------------------------\n");
		for (int i = DATA_START_SECTOR; i < DATA_START_SECTOR + (int)preview_data_sectors; i++) {
			printf("Sector %2d: ", i);
			for (int j = 0; j < 50; j++) {
				printf("%02X ", buf[i * SECTOR_SIZE + j]);
			}
			printf("\n");
		}

		free(buf);
	}
	 
	if(!strcmp(argv[1], "--reset_flash")){
		chip_erase();
        create_tables_wear_levering();
		create_inodes_table();

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

	// Загрузить из системы на флешку (Пример: ./main --import my_image.jpg)
    if(!strcmp(argv[1], "--import")){

        import_from_linux(argv[2]);
        close(fd);
        return 0;
    }

    // Выгрузить с флешки в систему (Пример: ./main --export my_image.jpg)
    if(!strcmp(argv[1], "--export")){
        export_to_linux(argv[2]);
        close(fd);
        return 0;
    }

	close(fd);
	return 0;
}
