#ifndef SIMPLEFS_H
#define SIMPLEFS_H

#include "../cpu/types.h"

#define SIMPLEFS_START_LBA 2048
#define SIMPLEFS_SECTOR_COUNT 9
#define SIMPLEFS_MAGIC 0x31534653
#define SIMPLEFS_VERSION 1

void simplefs_init();

int simplefs_format();
int simplefs_save();
int simplefs_load();

int simplefs_is_formatted();
int simplefs_get_disk_file_count();

u32 simplefs_get_start_lba();
u32 simplefs_get_sector_count();

#endif