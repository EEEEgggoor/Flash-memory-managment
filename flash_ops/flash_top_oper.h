#ifndef FLASH_TOP_OPER_H
#define FLASH_TOP_OPER_H

#include "flash_oper.h"
#include "file_system/extent.h"
#include "file_system/indode.h"

static inline void inc_note_sectors(uint32_t* addrs, uint32_t* lens, int mode, size_t len, int count_sectors_erases);
static inline int erase_range(uint32_t addr, size_t len, uint32_t** sectors_erases, uint32_t** bytes_per_sectors);


static inline void init_perez(uint32_t *buff){

	buff[0] = 0;
	
	for(int i = 1; i < WL_TABLE_ENTRIES; i++){ buff[i] = 0; }

}

static inline uint32_t read_uint32(uint32_t addr){
	uint8_t bytes[4];
	read_data(addr, bytes, 4);
	return ( ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | ((uint32_t)bytes[3]) );
}

static inline void raw_write_uint32(uint32_t addr, uint32_t value){
	uint8_t bytes[4] = {value >> 24, value >> 16, value >>8, value};
	page_program(addr, bytes, 4);
}

static inline int fragmentation_file(uint32_t *buff, size_t sectors_needed,
                                     extent_t *out, int *out_count, int max_extents){

	int run_start = -1;
	size_t run_len = 0;
	size_t collected = 0;
	int n = 0;

	for(int i = DATA_START_SECTOR*WL_ENTRIES_PER_SECTOR; i < WL_TABLE_ENTRIES; i += WL_ENTRIES_PER_SECTOR){
		if(buff[i] == 0){
			if(run_start == -1) { run_start = i; }
			run_len++;
		} else {
			if(run_len > 0){
				if(n >= max_extents) { return 0; }
				size_t take = run_len;
				if(collected + take > sectors_needed) take = sectors_needed - collected;
				out[n].addr = (run_start/WL_ENTRIES_PER_SECTOR) * SECTOR_SIZE;
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
		out[n].addr = (run_start/WL_ENTRIES_PER_SECTOR) * SECTOR_SIZE;
		out[n].len  = (uint32_t)take;
		collected += take;
		n++;
	}

	*out_count = n;
	return collected >= sectors_needed;
}


static inline int erase_range(uint32_t addr, size_t len, uint32_t** sectors_erases, uint32_t** bytes_per_sectors){

	uint32_t sector_start = addr & ~(SECTOR_SIZE - 1);
	uint32_t sector_end = (addr+len-1) & ~(SECTOR_SIZE - 1);

	int count = (int)((sector_end - sector_start)/SECTOR_SIZE) + 1;
	uint32_t* new_buf = (uint32_t*)realloc(*sectors_erases, count*sizeof(uint32_t));
	uint32_t* new_buf_bytes = (uint32_t*)realloc(*bytes_per_sectors, count*sizeof(uint32_t));

	size_t rem = len;
	uint32_t offset_in_sector = addr - sector_start;
	int k = 0;

	for(uint32_t s = sector_start; s <= sector_end; s += SECTOR_SIZE){

		size_t spase_here = SECTOR_SIZE - offset_in_sector;
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

	uint32_t index_max_gen = 0;
	uint32_t *buff = (uint32_t*)malloc(WL_TABLE_BYTES);
	if (!buff) return;

	read_wl_table(buff, &index_max_gen);

	uint32_t next_slot = (index_max_gen + 1) % WL_TABLE_SLOTS;

	for(int j = 0; j < count_sectors_erases; j++){
		uint32_t addr = addrs[j];
		uint32_t sector_number = NUMBER_SECTOR(addr);
		if(mode == 0){
			buff[sector_number * WL_ENTRIES_PER_SECTOR] = lens[j];
			buff[sector_number * WL_ENTRIES_PER_SECTOR + 1] += 1;
		}
		else{
			buff[sector_number * WL_ENTRIES_PER_SECTOR] += (uint32_t)len;
		}
	}

	buff[0] += 1; 

	write_wl_slot(next_slot, buff);

	free(buff);
}

static inline extent_list_t write_data(uint8_t *data, size_t len, uint32_t* dead_sectors) //запись в самую "свежую" ячейку
{
	uint32_t addr_sector;
	size_t writt = 0;
	uint32_t min_sector = 0xFFFFFFFF, min_sector_with_free_data = 0 /* <- переменная, хранящая свободное место у самого "свежего" сектора*/;
	int idx_min_sector = 0, flag = 0;

	extent_list_t _exts = {0}; 

	uint32_t *buff = (uint32_t*)malloc(WL_TABLE_BYTES);
	if (!buff) return (extent_list_t){0};

	read_wl_table(buff, NULL);

	for(int i = DATA_START_SECTOR*WL_ENTRIES_PER_SECTOR; i < WL_TABLE_ENTRIES; i += WL_ENTRIES_PER_SECTOR){
		if(SECTOR_SIZE - buff[i] >= len){
			flag = 1; //найден сектор, в который можно дозаписать данные
			if(buff[i+1] < min_sector){
				if(dead_sectors[i / WL_ENTRIES_PER_SECTOR] == 0){
					min_sector = buff[i+1];
					idx_min_sector = i;
					min_sector_with_free_data = buff[i];
				}
			}
		}
	}

	if(flag == 0){
		if(len <= SECTOR_SIZE){
			uint32_t* sectors_erases = NULL;
			uint32_t* bytes_per_sectors = NULL;

			extent_t *exts = (extent_t*)malloc(MAX_EXTENTS * sizeof(extent_t));

			for(int i = DATA_START_SECTOR*WL_ENTRIES_PER_SECTOR; i < WL_TABLE_ENTRIES; i += WL_ENTRIES_PER_SECTOR){
				uint32_t count = buff[i+1];
				if(count < min_sector){
					if(dead_sectors[i / WL_ENTRIES_PER_SECTOR] == 0){
						min_sector = count;
						idx_min_sector = i;
					}

				}
			}

			addr_sector = (idx_min_sector/WL_ENTRIES_PER_SECTOR) * SECTOR_SIZE;
			exts[0].addr = addr_sector;
			exts[0].len  = (uint32_t)len;

			_exts.extents = exts;
			_exts.count = 1;

			int count = erase_range(addr_sector, len, &sectors_erases, &bytes_per_sectors);

			while(writt < len){
				uint32_t page_offset = (addr_sector + writt) % PAGE_SIZE;
				size_t chunk = PAGE_SIZE - page_offset;
				if(chunk > (len - writt)) chunk = len - writt;

				page_program(addr_sector + writt, data + writt, chunk);
				writt += chunk;
			}

			inc_note_sectors(sectors_erases, bytes_per_sectors, MODE_ERASED, len, count);
			free(sectors_erases);
			free(bytes_per_sectors);
		}
		else{
			size_t sectors_needed = (len + SECTOR_SIZE - 1) / SECTOR_SIZE;

			extent_t *exts = (extent_t*)malloc(MAX_EXTENTS * sizeof(extent_t));
			int ext_count;

			if(!fragmentation_file(buff, sectors_needed, exts, &ext_count, MAX_EXTENTS)){
				free(exts);
				free(buff);
				return (extent_list_t){0};
			}

			size_t data_off = 0;
			for(int e = 0; e < ext_count; e++){
				size_t chunk_len = (size_t)exts[e].len * SECTOR_SIZE;
				if(chunk_len > len - data_off) chunk_len = len - data_off;

				uint32_t* sectors_erases = NULL;
				uint32_t* bytes_per_sectors = NULL;
				int count = erase_range(exts[e].addr, chunk_len, &sectors_erases, &bytes_per_sectors);

				size_t writt_local = 0;

				
				while(writt_local < chunk_len){
					uint32_t page_offset = (exts[e].addr + writt_local) % PAGE_SIZE;
					size_t chunk = PAGE_SIZE - page_offset;
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
		addr_sector = ((idx_min_sector/WL_ENTRIES_PER_SECTOR) * SECTOR_SIZE) + min_sector_with_free_data;

		extent_t *exts = (extent_t*)malloc(MAX_EXTENTS * sizeof(extent_t));

		exts[0].addr = addr_sector;
		exts[0].len  = (uint32_t)len;

		_exts.extents = exts;
		_exts.count = 1; 

		while(writt < len){
			uint32_t page_offset = (addr_sector + writt) % PAGE_SIZE;
			size_t chunk = PAGE_SIZE - page_offset;
			if(chunk > (len - writt)) chunk = len - writt;

			page_program(addr_sector + writt, data + writt, chunk);
			writt += chunk;
		}

		uint32_t single_addr = (idx_min_sector/WL_ENTRIES_PER_SECTOR) * SECTOR_SIZE;
		inc_note_sectors(&single_addr, NULL, MODE_APPENDED, len, 1);
	}

	free(buff);

	return _exts;
}

static inline void read_data_chunked(uint32_t addr, uint8_t *buf, size_t len){
    size_t rd = 0;
    while(rd < len){
        size_t chunk = BUF_SIZE;
        if(chunk > len - rd) chunk = len - rd;
        read_data(addr + rd, buf + rd, chunk);
        rd += chunk;
    }
}



static inline uint32_t read_wl_table(uint32_t *out_buf, uint32_t *out_slot_idx){
	uint32_t max_gen = 0;
	uint32_t index_max_gen = 0;

	for(int i = 0; i < WL_TABLE_SLOTS; i++){
		uint32_t gen = read_uint32(tables[i]);
		if(max_gen < gen){
			max_gen = gen;
			index_max_gen = i;
		}
	}

	read_data_chunked(tables[index_max_gen], (uint8_t *)out_buf, WL_TABLE_BYTES);
	for(int i = 0; i < WL_TABLE_ENTRIES; i++){ out_buf[i] = __builtin_bswap32(out_buf[i]); }

	if(out_slot_idx) { *out_slot_idx = index_max_gen; }

	return max_gen;
}

static inline void write_wl_slot(uint32_t slot_idx, const uint32_t *buff) {
    uint32_t base = tables[slot_idx];

    for (int i = 0; i < WL_TABLE_SECTORS_PER_SLOT; i++) {
        sector_erase(base + SECTOR_SIZE * i);
    }

    uint32_t *be_buff = (uint32_t*)malloc(WL_TABLE_BYTES);
    if (!be_buff) return;
    for (int i = 0; i < WL_TABLE_ENTRIES; i++) {
        be_buff[i] = __builtin_bswap32(buff[i]);
    }

    uint8_t *raw = (uint8_t*)be_buff;
    size_t total = WL_TABLE_BYTES;
    size_t written = 0;

    while (written < total) {
        uint32_t addr = base + written;
        uint32_t page_offset = addr % PAGE_SIZE;
        size_t chunk = PAGE_SIZE - page_offset;
        if (chunk > total - written) chunk = total - written;

        page_program(addr, raw + written, chunk);
        written += chunk;
    }

    free(be_buff);
}

#endif
