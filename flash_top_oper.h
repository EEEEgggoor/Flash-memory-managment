#ifndef FLASH_TOP_OPER_H
#define FLASH_TOP_OPER_H

#include "flash_oper.h"

static inline void inc_note_sectors(uint32_t* addrs, uint32_t* lens, int mode, size_t len, int count_sectors_erases);
static inline int erase_range(uint32_t addr, size_t len, uint32_t** sectors_erases, uint32_t** bytes_per_sectors);

static inline uint32_t read_uint32(uint32_t addr){
	uint8_t bytes[4];
	read_data(addr, bytes, 4);
	return ( ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | ((uint32_t)bytes[3]) );
}

static inline void raw_write_uint32(uint32_t addr, uint32_t value){
	uint8_t bytes[4] = {value >> 24, value >> 16, value >>8, value};
	page_program(addr, bytes, 4);

}


static inline int find_free_range(uint32_t *buff, size_t sectors_needed, uint32_t* out_start_s){
    int run_start = -1;
    size_t run_len = 0;

    for(int i = META_SECTORS*2; i < 2048; i += 2){
        if(buff[i] == 0){
            if(run_start == -1) run_start = i;
            run_len++;
            if(run_len >= sectors_needed){ *out_start_s = run_start; return 1; }
        } else {
            run_start = -1;
            run_len = 0;
        }
    }
    return 0;
}


static inline uint32_t write_data(uint8_t *data, size_t len) //запись в самую "свежую" ячейку
{

	uint32_t buff_gen[4];
	uint32_t addr_sector;
	size_t writt = 0;
	uint32_t max_gen = 0, index_max_gen = 0;
	uint32_t min_sector = 0xFFFFFFFF, min_sector_with_free_data = 0/* <- переменная, хранящая свободное место у самого "свежего" сектора*/;
	int idx_min_sector = 0, flag = 0;


	for(int i = 0; i < 4; i++){
		buff_gen[i] = read_uint32(tables[i]);
	}
	
	for(int i = 0; i < 4; i++){
		if(max_gen < buff_gen[i]) { max_gen = buff_gen[i]; index_max_gen = i; }
	}


	uint32_t *buff = (uint32_t*)malloc(2048*4);	//выгрузка свежей таблицы 
	for(int i = 0; i < 2048; i++){
		buff[i] = read_uint32(tables[index_max_gen] + 0x000004 * i);
	}


	for(int i = META_SECTORS*2; i < 2048; i += 2){
		if(4096 - buff[i] >= len){
			flag = 1; //найден сектор, в который можно дозаписать данные
			if(buff[i+1] < min_sector){
				min_sector = buff[i+1];
				idx_min_sector = i;
				min_sector_with_free_data = buff[i];
			}
		}

	}
	if(flag == 0){
        if(len<=4096){
            uint32_t* sectors_erases = NULL;
            uint32_t* bytes_per_sectors = NULL;

            for(int i = META_SECTORS*2; i < 2048; i += 2){
                uint32_t count = buff[i+1];
                if(count < min_sector){
                    min_sector = count;
                    idx_min_sector = i;
                }
            }

            addr_sector = (idx_min_sector/2) << 12;

            int count = erase_range(addr_sector, len, &sectors_erases, &bytes_per_sectors);

            while(writt<len){
                uint32_t page_offset = (addr_sector + writt) % 256;
                size_t chunk = 256 - page_offset;
                if(chunk > (len-writt)) chunk = len - writt;
                    
                page_program(addr_sector+writt, data+writt, chunk);
                writt+=chunk;
            }

            inc_note_sectors(sectors_erases, bytes_per_sectors, MODE_ERASED, len, count);
            free(sectors_erases);
            free(bytes_per_sectors);
        }

        else{
            uint32_t start_sector;
            size_t sectors_needed = (len+4095)/4096;
            if(!find_free_range(buff, sectors_needed, &start_sector)){
                free(buff);
                return 0xFFFFFFFF; // этот случай ведёт к реализации эксентов
            }
            uint32_t* sectors_erases = NULL;
            uint32_t* bytes_per_sectors = NULL;

            addr_sector = (start_sector/2) << 12;
            int count = erase_range(addr_sector, len, &sectors_erases, &bytes_per_sectors);
            while(writt<len){
                uint32_t page_offset = (addr_sector + writt) % 256;
                size_t chunk = 256 - page_offset;
                if(chunk > (len-writt)) chunk = len - writt;
                    
                page_program(addr_sector+writt, data+writt, chunk);
                writt+=chunk;
            }

            inc_note_sectors(sectors_erases, bytes_per_sectors, MODE_ERASED, len, count);
            free(sectors_erases);
            free(bytes_per_sectors);
        }
		
	}

	else{
		addr_sector = ((idx_min_sector/2) << 12) + min_sector_with_free_data;


		while(writt<len){
			uint32_t page_offset = (addr_sector + writt) % 256;
			size_t chunk = 256 - page_offset;
			if(chunk > (len-writt)) chunk = len - writt;
				
			page_program(addr_sector+writt, data+writt, chunk);
			writt+=chunk;
		}

        uint32_t single_addr = idx_min_sector/2 << 12;
 		inc_note_sectors(&single_addr, NULL, MODE_APPENDED, len, 1);

	}


	free(buff);

	return addr_sector;
}

static inline int erase_range(uint32_t addr, size_t len, uint32_t** sectors_erases, uint32_t** bytes_per_sectors){

	uint32_t sector_start = addr & ~(4095);
	uint32_t sector_end = (addr+len-1) & ~(4095);

    int count = (int)((sector_end - sector_start)/4096) + 1;
    uint32_t* new_buf = (uint32_t*)realloc(*sectors_erases, count*sizeof(uint32_t));
    uint32_t* new_buf_bytes = (uint32_t*)realloc(*bytes_per_sectors, count*sizeof(uint32_t));

    size_t rem = len;
    uint32_t offset_in_sector = addr - sector_start;
    int k = 0;

	for(uint32_t s = sector_start; s <= sector_end; s+=4096) { 

        size_t spase_here = 4096 - offset_in_sector;
        size_t take = (rem < spase_here) ? rem : spase_here;
		sector_erase(s);
        new_buf[k] = s;
        new_buf_bytes[k] = (uint32_t)take;

        rem -= take;
        offset_in_sector = 0;
        k++;
 	}

    *sectors_erases = new_buf; 
    *bytes_per_sectors = new_buf_bytes;
    
    return count;

}


static inline void inc_note_sectors(uint32_t* addrs, uint32_t* lens, int mode, size_t len, int count_sectors_erases){


    uint32_t buff_gen[4];
    for(int i = 0; i < 4; i++){
        buff_gen[i] = read_uint32(tables[i]);
    }

    uint32_t max_gen = 0;
    uint32_t index_max_gen = 0;
    for(int i = 0; i < 4; i++){
        if(max_gen < buff_gen[i]) { max_gen = buff_gen[i]; index_max_gen = i; }
    }

    uint32_t *buff = (uint32_t*)malloc(2048*4);	
    for(int i = 0; i < 2048; i++){
        buff[i] = read_uint32(tables[index_max_gen] + 0x000004 * i);
    }

    uint32_t next_slot = (index_max_gen + 1) % 4;	

    for(int j = 0; j < count_sectors_erases; j++){
        uint32_t addr = addrs[j];
        uint32_t sector_number = addr >> 12;
        if(mode == 0){
            buff[sector_number * 2] = lens[j];
            buff[sector_number * 2 + 1] += 1;
        }
        else{
            buff[sector_number * 2] += (uint32_t)len;
        }             
    }
        


    for(int i = 0; i < 2; i++){ sector_erase(tables[next_slot] + 4096*i); wait_busy(); }
    
    raw_write_uint32(tables[next_slot], buff[0] + 1);
    for(int i = 1; i < 2048; i++) { raw_write_uint32(tables[next_slot] + 0x000004 * i, buff[i]); }	
    wait_busy();

    free(buff);
}

#endif