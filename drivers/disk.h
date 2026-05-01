#ifndef DISK_H
#define DISK_H

#include "../cpu/types.h"

#define DISK_SECTOR_SIZE 512

void disk_init();

int disk_is_present();
int disk_read_sector(u32 lba, u8 *buffer);
int disk_write_sector(u32 lba, u8 *buffer);

int disk_get_last_error();
char *disk_get_status_text();

#endif