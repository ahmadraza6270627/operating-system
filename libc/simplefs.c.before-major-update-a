#include "simplefs.h"
#include "ramfs.h"
#include "string.h"
#include "../drivers/disk.h"

#define SIMPLEFS_HEADER_SECTOR SIMPLEFS_START_LBA
#define SIMPLEFS_CONTENT_START (SIMPLEFS_START_LBA + 1)

#define SIMPLEFS_HEADER_MAGIC_OFFSET 0
#define SIMPLEFS_HEADER_VERSION_OFFSET 4
#define SIMPLEFS_HEADER_MAX_FILES_OFFSET 8
#define SIMPLEFS_HEADER_FILE_COUNT_OFFSET 12
#define SIMPLEFS_ENTRY_START_OFFSET 16
#define SIMPLEFS_ENTRY_SIZE 24
#define SIMPLEFS_ENTRY_USED_OFFSET 0
#define SIMPLEFS_ENTRY_NAME_OFFSET 4
#define SIMPLEFS_ENTRY_SIZE_OFFSET 20

static u8 sector_buffer[DISK_SECTOR_SIZE];
static u8 content_buffer[DISK_SECTOR_SIZE];

static void zero_buffer(u8 *buffer, int size) {
    int i;

    for (i = 0; i < size; i++) {
        buffer[i] = 0;
    }
}

static void write_u32(u8 *buffer, int offset, u32 value) {
    buffer[offset] = (u8)(value & 0xFF);
    buffer[offset + 1] = (u8)((value >> 8) & 0xFF);
    buffer[offset + 2] = (u8)((value >> 16) & 0xFF);
    buffer[offset + 3] = (u8)((value >> 24) & 0xFF);
}

static u32 read_u32(u8 *buffer, int offset) {
    u32 value = 0;

    value |= buffer[offset];
    value |= ((u32)buffer[offset + 1]) << 8;
    value |= ((u32)buffer[offset + 2]) << 16;
    value |= ((u32)buffer[offset + 3]) << 24;

    return value;
}

static void copy_name_to_sector(u8 *buffer, int offset, char *name) {
    int i;

    for (i = 0; i < RAMFS_NAME_SIZE; i++) {
        buffer[offset + i] = 0;
    }

    i = 0;

    while (name[i] != '\0' && i < RAMFS_NAME_SIZE - 1) {
        buffer[offset + i] = name[i];
        i++;
    }
}

static void copy_sector_to_name(char *name, u8 *buffer, int offset) {
    int i;

    for (i = 0; i < RAMFS_NAME_SIZE - 1; i++) {
        name[i] = buffer[offset + i];

        if (name[i] == '\0') {
            return;
        }
    }

    name[RAMFS_NAME_SIZE - 1] = '\0';
}

static void copy_content_to_sector(u8 *buffer, char *content) {
    int i;

    for (i = 0; i < DISK_SECTOR_SIZE; i++) {
        buffer[i] = 0;
    }

    i = 0;

    while (content[i] != '\0' && i < RAMFS_CONTENT_SIZE - 1) {
        buffer[i] = content[i];
        i++;
    }
}

static void copy_sector_to_content(char *content, u8 *buffer, int size) {
    int i;

    if (size >= RAMFS_CONTENT_SIZE) {
        size = RAMFS_CONTENT_SIZE - 1;
    }

    for (i = 0; i < size; i++) {
        content[i] = buffer[i];
    }

    content[size] = '\0';
}

void simplefs_init() {
}

u32 simplefs_get_start_lba() {
    return SIMPLEFS_START_LBA;
}

u32 simplefs_get_sector_count() {
    return SIMPLEFS_SECTOR_COUNT;
}

int simplefs_is_formatted() {
    if (!disk_read_sector(SIMPLEFS_HEADER_SECTOR, sector_buffer)) {
        return 0;
    }

    if (read_u32(sector_buffer, SIMPLEFS_HEADER_MAGIC_OFFSET) != SIMPLEFS_MAGIC) {
        return 0;
    }

    if (read_u32(sector_buffer, SIMPLEFS_HEADER_VERSION_OFFSET) != SIMPLEFS_VERSION) {
        return 0;
    }

    return 1;
}

int simplefs_get_disk_file_count() {
    if (!disk_read_sector(SIMPLEFS_HEADER_SECTOR, sector_buffer)) {
        return -1;
    }

    if (read_u32(sector_buffer, SIMPLEFS_HEADER_MAGIC_OFFSET) != SIMPLEFS_MAGIC) {
        return -1;
    }

    return (int)read_u32(sector_buffer, SIMPLEFS_HEADER_FILE_COUNT_OFFSET);
}

int simplefs_format() {
    u32 i;

    zero_buffer(sector_buffer, DISK_SECTOR_SIZE);

    write_u32(sector_buffer, SIMPLEFS_HEADER_MAGIC_OFFSET, SIMPLEFS_MAGIC);
    write_u32(sector_buffer, SIMPLEFS_HEADER_VERSION_OFFSET, SIMPLEFS_VERSION);
    write_u32(sector_buffer, SIMPLEFS_HEADER_MAX_FILES_OFFSET, RAMFS_MAX_FILES);
    write_u32(sector_buffer, SIMPLEFS_HEADER_FILE_COUNT_OFFSET, 0);

    if (!disk_write_sector(SIMPLEFS_HEADER_SECTOR, sector_buffer)) {
        return 0;
    }

    zero_buffer(sector_buffer, DISK_SECTOR_SIZE);

    for (i = 1; i < SIMPLEFS_SECTOR_COUNT; i++) {
        if (!disk_write_sector(SIMPLEFS_START_LBA + i, sector_buffer)) {
            return 0;
        }
    }

    return 1;
}

int simplefs_save() {
    int i;
    int count;
    int size;
    int entry_offset;
    char name[RAMFS_NAME_SIZE];
    char *content;

    zero_buffer(sector_buffer, DISK_SECTOR_SIZE);

    write_u32(sector_buffer, SIMPLEFS_HEADER_MAGIC_OFFSET, SIMPLEFS_MAGIC);
    write_u32(sector_buffer, SIMPLEFS_HEADER_VERSION_OFFSET, SIMPLEFS_VERSION);
    write_u32(sector_buffer, SIMPLEFS_HEADER_MAX_FILES_OFFSET, RAMFS_MAX_FILES);

    count = ramfs_file_count();
    write_u32(sector_buffer, SIMPLEFS_HEADER_FILE_COUNT_OFFSET, count);

    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        entry_offset = SIMPLEFS_ENTRY_START_OFFSET + (i * SIMPLEFS_ENTRY_SIZE);

        if (i < count && ramfs_get_file_name(i, name, RAMFS_NAME_SIZE)) {
            content = ramfs_read(name);

            if (content == 0) {
                content = "";
            }

            size = strlen(content);

            write_u32(sector_buffer, entry_offset + SIMPLEFS_ENTRY_USED_OFFSET, 1);
            copy_name_to_sector(sector_buffer, entry_offset + SIMPLEFS_ENTRY_NAME_OFFSET, name);
            write_u32(sector_buffer, entry_offset + SIMPLEFS_ENTRY_SIZE_OFFSET, size);

            copy_content_to_sector(content_buffer, content);

            if (!disk_write_sector(SIMPLEFS_CONTENT_START + i, content_buffer)) {
                return 0;
            }
        } else {
            write_u32(sector_buffer, entry_offset + SIMPLEFS_ENTRY_USED_OFFSET, 0);
            zero_buffer(content_buffer, DISK_SECTOR_SIZE);

            if (!disk_write_sector(SIMPLEFS_CONTENT_START + i, content_buffer)) {
                return 0;
            }
        }
    }

    if (!disk_write_sector(SIMPLEFS_HEADER_SECTOR, sector_buffer)) {
        return 0;
    }

    return 1;
}

int simplefs_load() {
    int i;
    int entry_offset;
    int used;
    int size;
    char name[RAMFS_NAME_SIZE];
    char content[RAMFS_CONTENT_SIZE];

    if (!disk_read_sector(SIMPLEFS_HEADER_SECTOR, sector_buffer)) {
        return -1;
    }

    if (read_u32(sector_buffer, SIMPLEFS_HEADER_MAGIC_OFFSET) != SIMPLEFS_MAGIC) {
        return -2;
    }

    if (read_u32(sector_buffer, SIMPLEFS_HEADER_VERSION_OFFSET) != SIMPLEFS_VERSION) {
        return -3;
    }

    ramfs_reset();

    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        entry_offset = SIMPLEFS_ENTRY_START_OFFSET + (i * SIMPLEFS_ENTRY_SIZE);
        used = (int)read_u32(sector_buffer, entry_offset + SIMPLEFS_ENTRY_USED_OFFSET);

        if (used) {
            copy_sector_to_name(name, sector_buffer, entry_offset + SIMPLEFS_ENTRY_NAME_OFFSET);
            size = (int)read_u32(sector_buffer, entry_offset + SIMPLEFS_ENTRY_SIZE_OFFSET);

            if (!disk_read_sector(SIMPLEFS_CONTENT_START + i, content_buffer)) {
                return -1;
            }

            copy_sector_to_content(content, content_buffer, size);

            if (ramfs_create(name) == 1) {
                ramfs_write(name, content);
            }
        }
    }

    return 1;
}