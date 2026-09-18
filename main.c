#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <time.h>

#include "flash_top_oper.h"


uint8_t r_hex(){ return (uint8_t)(rand() & 0xFF); }

void init_perez(uint32_t *buff){

	int k = 0;
		
	buff[0] = 0;
	
	for(int i = 1; i < 2048; i++){ buff[i] = 0; }

}

void create_tables(){


	uint32_t buff_table1[2048];
	init_perez(buff_table1);


	for(int i = 0; i < 4; i++){
		sector_erase(tables[i]); wait_busy();
		sector_erase(tables[i] + 4096); wait_busy();
		for(int j = 0; j < 2048; j++){
			raw_write_uint32(tables[i] + j * 0x000004, buff_table1[j]);
		}
	}



	for(int j = 0; j < 4; j++){
		for(int i = 0; i < 2048; i++) {
			buff_table[j][i] = read_uint32(tables[j] + 0x000004 * i);
		}
		wait_busy();
	}
	

}




int main(int argc, char *argv[]){

	srand(time(NULL));
	
	int num = atoi(argv[1]);

	flash_init(SPI_PATH, SPI_MODE_0, 8, 20000000);




	if(num != -1){

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

	
	close(fd);
	return 0;
}
