#ifndef RAMFS_H
#define RAMFS_H

#define RAMFS_MAX_FILES 8
#define RAMFS_NAME_SIZE 16
#define RAMFS_CONTENT_SIZE 256

void ramfs_init();
void ramfs_reset();

int ramfs_create(char *name);
int ramfs_write(char *name, char *content);
int ramfs_append(char *name, char *content);
int ramfs_delete(char *name);

char *ramfs_read(char *name);

int ramfs_exists(char *name);
int ramfs_file_count();
int ramfs_get_file_name(int index, char *buffer, int max_len);

int ramfs_size(char *name);
int ramfs_clear(char *name);
int ramfs_rename(char *old_name, char *new_name);
int ramfs_copy(char *src_name, char *dst_name);

#endif