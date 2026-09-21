#ifndef FS_FOPS_H
#define FS_FOPS_H

#include "flash_ops/flash_top_oper.h"


static inline int garbage_collection();

static inline void read_extents(extent_list_t *ext, uint8_t* buf, size_t total_len){

	size_t count = ext->count;
	size_t off = 0;

	for(size_t i = 0; i < count && off < total_len; i++){
		size_t take = ext->extents[i].len;
		if(take > total_len - off) take = total_len - off;

		read_data_chunked(ext->extents[i].addr, buf + off, take);
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

static inline void create_inodes_table(){

    file_inode_t empty_table[MAX_FILES];
    memset(empty_table, 0, sizeof(empty_table)); // все flags = 0, все поля обнулены

    for(int slot = 0; slot < 4; slot++){
        uint32_t base = inodes[slot];

        for(int i = 0; i < INODE_TABLE_TOTAL_SECTORS; i++){
            sector_erase(base + i * 4096);
            wait_busy();
        }

        raw_write_uint32(base, 0); 

        uint8_t *raw = (uint8_t*)empty_table;
        size_t total = INODE_TABLE_DATA_BYTES;
        size_t written = 0;
        while(written < total){
            uint32_t addr = base + INODE_TABLE_HEADER_BYTES + written;//резервируем сектор под что-то
            uint32_t page_offset = addr % 256;
            size_t chunk = 256 - page_offset;
            if(chunk > total - written) chunk = total - written;

            page_program(addr, raw + written, chunk);
            written += chunk;
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
	size_t written = 0;
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
    uint8_t *raw = (uint8_t*)out_table;
    size_t total = INODE_TABLE_DATA_BYTES;
    size_t rd = 0;

    while(rd < total){
        size_t chunk = 256;
        if(chunk > total - rd) chunk = total - rd;

        read_data_chunked(base + rd, raw + rd, chunk);
        rd += chunk;
    }
}

static inline int write_file(char *file_name, uint8_t *data, size_t len){
	file_inode_t table_inode[MAX_FILES];

	read_inode_table(table_inode);
	int slote = -1;
	for(int i = 0; i < MAX_FILES; i++){
		if(table_inode[i].flags != 1){ slote = i; break; }
	} 
	if(slote == -1) { return -1; }

	extent_list_t ext = write_data(data, len, dead_sectors);
	if(ext.count == 0 || ext.extents == NULL) { return -1; }

	if(ext.count > MAX_EXTENTS){
        free(ext.extents);
        return -1;
    }

	strncpy(table_inode[slote].name, file_name, sizeof(table_inode[slote].name) - 1);
	table_inode[slote].size = (uint32_t)len;
	table_inode[slote].ext_count = ext.count;
	memcpy(table_inode[slote].extents, ext.extents, ext.count * sizeof(extent_t));
	table_inode[slote].ctime = (uint32_t)time(NULL);
	table_inode[slote].flags = 1;

	free(ext.extents);

	write_inode_table(table_inode);

	return slote;
}


static inline int read_file(char* file_name, uint8_t *out_buf, size_t max_len){
	file_inode_t table_inode[MAX_FILES];
	int slote = -1;

	read_inode_table(table_inode);

	for(int i = 0; i <  MAX_FILES; i++){
		if(!strcmp(table_inode[i].name, file_name) && table_inode[i].flags == 1){
			extent_list_t ext;
            ext.count = table_inode[i].ext_count;
            ext.extents = table_inode[i].extents;

			size_t to_read = table_inode[i].size;

			if(to_read > max_len) to_read = max_len;

			read_extents(&ext, out_buf, to_read);
			return (int)table_inode[i].size;
		}
	}
	return -1;
}

static inline int delete_file(char* file_name){

	file_inode_t table_inode[MAX_FILES];

	read_inode_table(table_inode);
	for(int i = 0; i < MAX_FILES; i++){
		if(!strcmp(table_inode[i].name, file_name) && table_inode[i].flags == 1){
			table_inode[i].flags = 5;
			write_inode_table(table_inode);
			return 0; 
			garbage_collection();
		}
	}


	return -1;
}

static inline int garbage_collection(){
	file_inode_t table_inode[MAX_FILES];
	read_inode_table(table_inode);

	uint32_t alives_bytes[2048] = {0};



	for(int i = 0; i < MAX_FILES; i++){
		if(table_inode[i].flags == 1){
			for(int j = 0; j < table_inode[i].ext_count; j++){
				uint32_t sector_num = NUMBER_SECTOR(table_inode[i].extents[j].addr);
				alives_bytes[sector_num] += table_inode[i].extents[j].len; //подсчет, сколько живих байт в секторе
			}
		}
	}


	uint32_t wl_table[4][2048];
	for(int k = 0; k < 4; k ++){
		for(int i = 0; i < 2048; i++){
			wl_table[k][i] = read_uint32(tables[k] + 4*i);
		}
	}

	uint32_t max_gen = 0;
	uint32_t index_max_gen = 0;
	for(int i = 0; i < 4; i++){
		if(max_gen < wl_table[i][0]) { max_gen = wl_table[i][0]; index_max_gen = i; }
	}



	for(int i = DATA_START_SECTOR; i < 1024; i++){
		uint32_t total_writt = wl_table[index_max_gen][i * 2];

		if(total_writt > 0){

			uint32_t dead_bytes = total_writt - alives_bytes[i];

			float dead_sector_proc = dead_bytes/4096.0;
			if(dead_sector_proc >= 0.90f){
				dead_sectors[i] = 1;
			}
			else{
				dead_sectors[i] = 0;
			}
		}
	}


	for(int i = 0; i < MAX_FILES; i++){
		if(table_inode[i].flags == 1){

			int needs_moving = 0;
			for(int j = 0; j < table_inode[i].ext_count; j++){
				uint32_t sec = NUMBER_SECTOR(table_inode[i].extents[j].addr);
				if(dead_sectors[sec] == 1){
					needs_moving = 1;
				}
			}

			if (needs_moving){
				uint8_t *data_buf = (uint8_t*)malloc(table_inode[i].size);
				if (!data_buf) continue;

				read_file(table_inode[i].name, data_buf, table_inode[i].size);
				extent_list_t ext = write_data(data_buf, table_inode[i].size, dead_sectors);

				table_inode[i].ext_count = ext.count;
				memcpy(table_inode[i].extents, ext.extents, ext.count * sizeof(extent_t));

				free(ext.extents);
				free(data_buf);  
			}


		}
	}

	write_inode_table(table_inode);

	for(int s = DATA_START_SECTOR; s < 1024; s++){
		if(dead_sectors[s] == 1) {
			
			uint32_t erased_addr = s * 4096;
			sector_erase(erased_addr);
			
			uint32_t zero_bytes = 0;
			inc_note_sectors(&erased_addr, &zero_bytes, MODE_ERASED, 0, 1); 

			dead_sectors[s] = 0;
		}
	}

}

#endif //FS_FOPS