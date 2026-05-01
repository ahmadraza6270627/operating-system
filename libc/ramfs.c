#include "ramfs.h"
#include "string.h"

typedef struct {
    int used;
    char name[RAMFS_NAME_SIZE];
    char content[RAMFS_CONTENT_SIZE];
} ramfs_file_t;

static ramfs_file_t files[RAMFS_MAX_FILES];

static int valid_name(char *name) {
    int len;

    if (name[0] == '\0') {
        return 0;
    }

    len = strlen(name);

    if (len >= RAMFS_NAME_SIZE) {
        return 0;
    }

    return 1;
}

static void clear_file(int index) {
    files[index].used = 0;
    files[index].name[0] = '\0';
    files[index].content[0] = '\0';
}

static int find_file(char *name) {
    int i;

    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used) {
            if (strcmp(files[i].name, name) == 0) {
                return i;
            }
        }
    }

    return -1;
}

static int find_free_file() {
    int i;

    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].used) {
            return i;
        }
    }

    return -1;
}

static void copy_limited(char *dest, char *src, int max_len) {
    int i = 0;

    while (src[i] != '\0' && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }

    dest[i] = '\0';
}

void ramfs_init() {
    ramfs_reset();
}

void ramfs_reset() {
    int i;

    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        clear_file(i);
    }
}

int ramfs_create(char *name) {
    int index;

    if (!valid_name(name)) {
        return -1;
    }

    if (find_file(name) >= 0) {
        return -2;
    }

    index = find_free_file();

    if (index < 0) {
        return -3;
    }

    files[index].used = 1;
    copy_limited(files[index].name, name, RAMFS_NAME_SIZE);
    files[index].content[0] = '\0';

    return 1;
}

int ramfs_write(char *name, char *content) {
    int index;

    index = find_file(name);

    if (index < 0) {
        return -1;
    }

    if (strlen(content) >= RAMFS_CONTENT_SIZE) {
        return -2;
    }

    copy_limited(files[index].content, content, RAMFS_CONTENT_SIZE);

    return 1;
}

int ramfs_append(char *name, char *content) {
    int index;
    int current_len;
    int add_len;

    index = find_file(name);

    if (index < 0) {
        return -1;
    }

    current_len = strlen(files[index].content);
    add_len = strlen(content);

    if (current_len + add_len >= RAMFS_CONTENT_SIZE) {
        return -2;
    }

    strcat(files[index].content, content);

    return 1;
}

int ramfs_delete(char *name) {
    int index;

    index = find_file(name);

    if (index < 0) {
        return -1;
    }

    clear_file(index);

    return 1;
}

char *ramfs_read(char *name) {
    int index;

    index = find_file(name);

    if (index < 0) {
        return 0;
    }

    return files[index].content;
}

int ramfs_exists(char *name) {
    if (find_file(name) >= 0) {
        return 1;
    }

    return 0;
}

int ramfs_file_count() {
    int i;
    int count = 0;

    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used) {
            count++;
        }
    }

    return count;
}

int ramfs_get_file_name(int index, char *buffer, int max_len) {
    int i;
    int found = 0;

    for (i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used) {
            if (found == index) {
                copy_limited(buffer, files[i].name, max_len);
                return 1;
            }

            found++;
        }
    }

    return 0;
}

int ramfs_size(char *name) {
    int index;

    index = find_file(name);

    if (index < 0) {
        return -1;
    }

    return strlen(files[index].content);
}

int ramfs_clear(char *name) {
    int index;

    index = find_file(name);

    if (index < 0) {
        return -1;
    }

    files[index].content[0] = '\0';

    return 1;
}

int ramfs_rename(char *old_name, char *new_name) {
    int index;

    if (!valid_name(new_name)) {
        return -2;
    }

    index = find_file(old_name);

    if (index < 0) {
        return -1;
    }

    if (find_file(new_name) >= 0) {
        return -3;
    }

    copy_limited(files[index].name, new_name, RAMFS_NAME_SIZE);

    return 1;
}

int ramfs_copy(char *src_name, char *dst_name) {
    int src_index;
    int dst_index;

    if (!valid_name(dst_name)) {
        return -2;
    }

    src_index = find_file(src_name);

    if (src_index < 0) {
        return -1;
    }

    if (find_file(dst_name) >= 0) {
        return -3;
    }

    dst_index = find_free_file();

    if (dst_index < 0) {
        return -4;
    }

    files[dst_index].used = 1;
    copy_limited(files[dst_index].name, dst_name, RAMFS_NAME_SIZE);
    copy_limited(files[dst_index].content, files[src_index].content, RAMFS_CONTENT_SIZE);

    return 1;
}