#include "file_system/FS_fops.h"


#define LOG_INFO(fmt, ...) printf("[ INFO ] " fmt "\n", ##__VA_ARGS__)
#define LOG_PASS(fmt, ...) printf("[ PASS ] " fmt "\n", ##__VA_ARGS__)
#define LOG_FAIL(fmt, ...) printf("[ FAIL ] " fmt "\n", ##__VA_ARGS__)

static inline int run_fs_tests() {
    LOG_INFO("Starting filesystem integration tests");
    
    LOG_INFO("Formatting flash memory...");
    create_tables_wear_levering();
    create_inodes_table();
    
    LOG_INFO("Test 1: Writing 5000 bytes to 'file1.bin'");
    uint8_t write_buf1[5000];
    memset(write_buf1, 0xAA, sizeof(write_buf1));
    if (write_file("file1.bin", write_buf1, sizeof(write_buf1)) < 0) {
        LOG_FAIL("Failed to write 'file1.bin'");
        return -1;
    }
    
    LOG_INFO("Test 2: Verifying 'file1.bin' integrity");
    uint8_t read_buf1[5000];
    memset(read_buf1, 0, sizeof(read_buf1));
    int read_len = read_file("file1.bin", read_buf1, sizeof(read_buf1));
    if (read_len != (int)sizeof(read_buf1)) {
        LOG_FAIL("Read size mismatch (expected %zu, got %d)", sizeof(read_buf1), read_len);
        return -1;
    }
    for (size_t i = 0; i < sizeof(read_buf1); i++) {
        if (read_buf1[i] != 0xAA) {
            LOG_FAIL("Data corruption at offset %zu (expected 0xAA, got 0x%02X)", i, read_buf1[i]);
            return -1;
        }
    }
    LOG_PASS("'file1.bin' verified successfully");
    
    LOG_INFO("Test 3: Writing fragmented file 'file2.bin' (15000 bytes)");
    uint8_t write_buf2[15000];
    memset(write_buf2, 0xBB, sizeof(write_buf2));
    if (write_file("file2.bin", write_buf2, sizeof(write_buf2)) < 0) {
        LOG_FAIL("Failed to write 'file2.bin'");
        return -1;
    }
    
    LOG_INFO("Test 4: Deleting 'file1.bin' to simulate invalid extents");
    if (delete_file("file1.bin") != 0) {
        LOG_FAIL("Failed to delete 'file1.bin'");
        return -1;
    }
    
    LOG_INFO("Test 5: Writing 'file3.bin' (3000 bytes) into fragmented space");
    uint8_t write_buf3[3000];
    memset(write_buf3, 0xCC, sizeof(write_buf3));
    if (write_file("file3.bin", write_buf3, sizeof(write_buf3)) < 0) {
        LOG_FAIL("Failed to write 'file3.bin'");
        return -1;
    }
    
    LOG_INFO("Test 6: Triggering Garbage Collection");
    garbage_collection();
    
    LOG_INFO("Test 7: Verifying 'file2.bin' integrity post-GC");
    uint8_t read_buf2[15000];
    if (read_file("file2.bin", read_buf2, sizeof(read_buf2)) != (int)sizeof(read_buf2)) {
        LOG_FAIL("Failed to read 'file2.bin' after GC");
        return -1;
    }
    for (size_t i = 0; i < sizeof(read_buf2); i++) {
        if (read_buf2[i] != 0xBB) {
            LOG_FAIL("'file2.bin' corrupted by GC at offset %zu", i);
            return -1;
        }
    }
    LOG_PASS("'file2.bin' verified successfully");
    
    LOG_INFO("Test 8: Verifying 'file3.bin' integrity post-GC");
    uint8_t read_buf3[3000];
    if (read_file("file3.bin", read_buf3, sizeof(read_buf3)) != (int)sizeof(read_buf3)) {
        LOG_FAIL("Failed to read 'file3.bin' after GC");
        return -1;
    }
    for (size_t i = 0; i < sizeof(read_buf3); i++) {
        if (read_buf3[i] != 0xCC) {
            LOG_FAIL("'file3.bin' corrupted by GC at offset %zu", i);
            return -1;
        }
    }
    LOG_PASS("'file3.bin' verified successfully");
    
    LOG_PASS("All filesystem tests completed successfully.");
    return 0;
}