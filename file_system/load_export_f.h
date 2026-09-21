#include "FS_fops.h"

static int import_from_linux(const char *linux_file) {
    FILE *f = fopen(linux_file, "rb");
    if (!f) {
        perror("[err] open file");
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        printf("[err] file size = 0\n");
        fclose(f);
        return -1;
    }

    uint8_t *buf = (uint8_t *)malloc(file_size);
    if (!buf) {
        printf("[err]\n");
        fclose(f);
        return -1;
    }

    if (fread(buf, 1, file_size, f) != (size_t)file_size) {
        printf("[err] read file from linux\n");
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);


    int res = write_file((char*)linux_file, buf, file_size);
    free(buf);

    if (res >= 0) {
        printf("[done]");
        return 0;
    } else {
        printf("[err] write in flash\n");
        return -1;
    }
}

static int export_to_linux(char *flash_file) {
    file_inode_t table_inode[MAX_FILES];
    read_inode_table(table_inode);
    
    int size = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (table_inode[i].flags == 1 && !strcmp(table_inode[i].name, flash_file)) {
            size = table_inode[i].size;
            break;
        }
    }

    if (size == -1) {
        printf("[err] file '%s' not found\n", flash_file);
        return -1;
    }

    uint8_t *buf = (uint8_t *)malloc(size);
    if (!buf) {
        printf("[err]\n");
        return -1;
    }

    int read_len = read_file(flash_file, buf, size);
    if (read_len != size) {
        printf("[err] fail read file\n");
        free(buf);
        return -1;
    }

    FILE *f = fopen(flash_file, "wb");
    if (!f) {
        perror("[err] create file");
        free(buf);
        return -1;
    }

    if (fwrite(buf, 1, size, f) != (size_t)size) {
        printf("[err] write on system\n");
        fclose(f);
        free(buf);
        return -1;
    }

    fclose(f);
    free(buf);
    
    printf("[done]");
    return 0;
}