#ifndef _INIT_H
#define _INIT_H
#define _INIT_H

#include "flash_ops/flash_top_oper.h"

static inline int fragmentation_file(uint32_t *buff, size_t sectors_needed,
                                     extent_t *out, int *out_count, int max_extents){

	int run_start = -1;
	size_t run_len = 0;
	size_t collected = 0;
	int n = 0;

	for(int i = META_SECTORS*2; i < 2048; i += 2){
		if(buff[i] == 0){
			if(run_start == -1) { run_start = i; }
			run_len++;
		} else {
			if(run_len > 0){
				if(n >= max_extents) { return 0; }
				size_t take = run_len;
				if(collected + take > sectors_needed) take = sectors_needed - collected;
				out[n].addr = (run_start/2) << 12;
				out[n].len  = (uint32_t)take;
				collected += take;
				n++;
			}
			run_start = -1;
			run_len = 0;
		}

		if(collected >= sectors_needed) break;
	}

	if(n < max_extents && collected < sectors_needed && run_len > 0){
		size_t take = run_len;
		if(collected + take > sectors_needed) take = sectors_needed - collected;
		out[n].addr = (run_start/2) << 12;
		out[n].len  = (uint32_t)take;
		collected += take;
		n++;
	}

	*out_count = n;
	return collected >= sectors_needed;
}


static inline void read_extents(extent_list_t *ext, uint8_t* buf, size_t total_len){

	size_t count = ext->count;
	size_t off = 0;

	for(size_t i = 0; i < count && off < total_len; i++){
		size_t take = ext->extents[i].len;
		if(take > total_len - off) take = total_len - off;

		read_data(ext->extents[i].addr, buf + off, take);
		off += take;
	}
}

static inline void create_tables_wear_levering(){


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

static inline void write_inode_table(file_inode_t *table){
	uint32_t buff_gen[4];
	for(int i = 0; i < 4; i++){
		buff_gen[i] = read_uint32(inodes[i]);
	}

	uint32_t max_gen = 0;
	uint32_t index_max_gen = 0;
	for(int i = 0; i < 4; i++){
		if(max_gen < buff_gen[i]) { max_gen = buff_gen[i]; index_max_gen = i; }
	}

	uint32_t *buff = (uint32_t*)malloc(2048*4);
	for(int i = 0; i < 2048; i++){
		buff[i] = read_uint32(inodes[index_max_gen] + 0x000004 * i);
	}

	uint32_t next_slot = (index_max_gen + 1) % 4;

	uint32_t base = inodes[next_slot];

	for(int i = 0; i < INODE_TABLE_TOTAL_SECTORS; i++){
		sector_erase(base + i * 4096);
	}

	raw_write_uint32(base, max_gen + 1);

	uint8_t *raw = (uint8_t*)table;
	size_t total = INODE_TABLE_DATA_BYTES;
	size_t written;
	while(written < total){
		uint32_t addr = base + INODE_TABLE_HEADER_BYTES + written;
		uint32_t page_offset = addr%256;
		size_t chunk = 256 - page_offset;

		if(chunk > total - written) { chunk = total - written; }

		page_program(addr, raw + written, chunk);

		written+=chunk;
	}
	wait_busy();

}

static inline void read_inode_table(file_inode_t *out_table){

    uint32_t buff_gen[4];
    for(int i = 0; i < 4; i++){
        buff_gen[i] = read_uint32(inodes[i]);
    }

    uint32_t max_gen = 0, index_max_gen = 0;
    for(int i = 0; i < 4; i++){
        if(max_gen < buff_gen[i]) { max_gen = buff_gen[i]; index_max_gen = i; }
    }

    uint32_t base = inodes[index_max_gen] + INODE_TABLE_HEADER_BYTES;
    read_data(base, (uint8_t*)out_table, INODE_TABLE_DATA_BYTES);
}

static inline extent_list_t write_data(uint8_t *data, size_t len) //запись в самую "свежую" ячейку
{
	uint32_t buff_gen[4];
	uint32_t addr_sector;
	size_t writt = 0;
	uint32_t max_gen = 0, index_max_gen = 0;
	uint32_t min_sector = 0xFFFFFFFF, min_sector_with_free_data = 0 /* <- переменная, хранящая свободное место у самого "свежего" сектора*/;
	int idx_min_sector = 0, flag = 0;

	extent_list_t _exts = {0}; 

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
		if(len <= 4096){
			uint32_t* sectors_erases = NULL;
			uint32_t* bytes_per_sectors = NULL;

			extent_t *exts = (extent_t*)malloc(MAX_EXTENTS * sizeof(extent_t));

			for(int i = META_SECTORS*2; i < 2048; i += 2){
				uint32_t count = buff[i+1];
				if(count < min_sector){
					min_sector = count;
					idx_min_sector = i;
				}
			}

			addr_sector = (idx_min_sector/2) << 12;
			exts[0].addr = addr_sector;
			exts[0].len  = (uint32_t)len;

			_exts.extents = exts;
			_exts.count = 1;

			int count = erase_range(addr_sector, len, &sectors_erases, &bytes_per_sectors);

			while(writt < len){
				uint32_t page_offset = (addr_sector + writt) % 256;
				size_t chunk = 256 - page_offset;
				if(chunk > (len - writt)) chunk = len - writt;

				page_program(addr_sector + writt, data + writt, chunk);
				writt += chunk;
			}

			inc_note_sectors(sectors_erases, bytes_per_sectors, MODE_ERASED, len, count);
			free(sectors_erases);
			free(bytes_per_sectors);
		}
		else{
			size_t sectors_needed = (len + 4095) / 4096;

			extent_t *exts = (extent_t*)malloc(MAX_EXTENTS * sizeof(extent_t));
			int ext_count;

			if(!fragmentation_file(buff, sectors_needed, exts, &ext_count, MAX_EXTENTS)){
				free(exts);
				free(buff);
				return (extent_list_t){0};
			}

			size_t data_off = 0;
			for(int e = 0; e < ext_count; e++){
				size_t chunk_len = (size_t)exts[e].len * 4096;
				if(chunk_len > len - data_off) chunk_len = len - data_off;

				uint32_t* sectors_erases = NULL;
				uint32_t* bytes_per_sectors = NULL;
				int count = erase_range(exts[e].addr, chunk_len, &sectors_erases, &bytes_per_sectors);

				size_t writt_local = 0;

				
				while(writt_local < chunk_len){
					uint32_t page_offset = (exts[e].addr + writt_local) % 256;
					size_t chunk = 256 - page_offset;
					if(chunk > chunk_len - writt_local) chunk = chunk_len - writt_local;

					page_program(exts[e].addr + writt_local, data + data_off + writt_local, chunk);
					writt_local += chunk;
				}

				inc_note_sectors(sectors_erases, bytes_per_sectors, MODE_ERASED, chunk_len, count);
				free(sectors_erases);
				free(bytes_per_sectors);

				exts[e].len = (uint32_t)chunk_len;
				data_off += chunk_len;
			}

			_exts.extents = exts;
			_exts.count = ext_count;
		}
	}
	else{
		addr_sector = ((idx_min_sector/2) << 12) + min_sector_with_free_data;

		extent_t *exts = (extent_t*)malloc(MAX_EXTENTS * sizeof(extent_t));

		exts[0].addr = addr_sector;
		exts[0].len  = (uint32_t)len;

		_exts.extents = exts;
		_exts.count = 1; 

		while(writt < len){
			uint32_t page_offset = (addr_sector + writt) % 256;
			size_t chunk = 256 - page_offset;
			if(chunk > (len - writt)) chunk = len - writt;

			page_program(addr_sector + writt, data + writt, chunk);
			writt += chunk;
		}

		uint32_t single_addr = idx_min_sector/2 << 12;
		inc_note_sectors(&single_addr, NULL, MODE_APPENDED, len, 1);
	}

	free(buff);

	return _exts;
}

#endif //_INIT_H