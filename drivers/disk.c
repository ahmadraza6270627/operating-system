#include "disk.h"
#include "../cpu/ports.h"

#define ATA_DATA        0x1F0
#define ATA_ERROR       0x1F1
#define ATA_SECTOR_CNT  0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE       0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7
#define ATA_ALT_STATUS  0x3F6
#define ATA_CONTROL     0x3F6

#define ATA_CMD_READ_SECTORS   0x20
#define ATA_CMD_WRITE_SECTORS  0x30
#define ATA_CMD_CACHE_FLUSH    0xE7

#define ATA_STATUS_ERR  0x01
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_DF   0x20
#define ATA_STATUS_BSY  0x80

#define DISK_ERROR_NONE        0
#define DISK_ERROR_TIMEOUT     1
#define DISK_ERROR_ATA         2
#define DISK_ERROR_BAD_LBA     3

static int disk_present = 0;
static int disk_last_error = DISK_ERROR_NONE;

static void ata_io_wait() {
    port_byte_in(ATA_ALT_STATUS);
    port_byte_in(ATA_ALT_STATUS);
    port_byte_in(ATA_ALT_STATUS);
    port_byte_in(ATA_ALT_STATUS);
}

static int ata_wait_not_busy() {
    int i;
    u8 status;

    for (i = 0; i < 100000; i++) {
        status = port_byte_in(ATA_STATUS);

        if ((status & ATA_STATUS_BSY) == 0) {
            return 1;
        }
    }

    disk_last_error = DISK_ERROR_TIMEOUT;
    return 0;
}

static int ata_wait_drq() {
    int i;
    u8 status;

    for (i = 0; i < 100000; i++) {
        status = port_byte_in(ATA_STATUS);

        if (status & ATA_STATUS_ERR) {
            disk_last_error = DISK_ERROR_ATA;
            return 0;
        }

        if (status & ATA_STATUS_DF) {
            disk_last_error = DISK_ERROR_ATA;
            return 0;
        }

        if ((status & ATA_STATUS_BSY) == 0 && (status & ATA_STATUS_DRQ)) {
            return 1;
        }
    }

    disk_last_error = DISK_ERROR_TIMEOUT;
    return 0;
}

static void ata_select_drive(u32 lba) {
    port_byte_out(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    ata_io_wait();
}

void disk_init() {
    disk_present = 1;
    disk_last_error = DISK_ERROR_NONE;

    port_byte_out(ATA_CONTROL, 0x00);
    ata_io_wait();
}

int disk_is_present() {
    return disk_present;
}

int disk_read_sector(u32 lba, u8 *buffer) {
    int i;
    u16 word;

    if (lba > 0x0FFFFFFF) {
        disk_last_error = DISK_ERROR_BAD_LBA;
        return 0;
    }

    if (!ata_wait_not_busy()) {
        return 0;
    }

    ata_select_drive(lba);

    port_byte_out(ATA_SECTOR_CNT, 1);
    port_byte_out(ATA_LBA_LOW, (u8)(lba & 0xFF));
    port_byte_out(ATA_LBA_MID, (u8)((lba >> 8) & 0xFF));
    port_byte_out(ATA_LBA_HIGH, (u8)((lba >> 16) & 0xFF));
    port_byte_out(ATA_COMMAND, ATA_CMD_READ_SECTORS);

    if (!ata_wait_drq()) {
        return 0;
    }

    for (i = 0; i < 256; i++) {
        word = port_word_in(ATA_DATA);

        buffer[i * 2] = (u8)(word & 0xFF);
        buffer[i * 2 + 1] = (u8)((word >> 8) & 0xFF);
    }

    ata_io_wait();

    disk_last_error = DISK_ERROR_NONE;
    return 1;
}

int disk_write_sector(u32 lba, u8 *buffer) {
    int i;
    u16 word;

    if (lba > 0x0FFFFFFF) {
        disk_last_error = DISK_ERROR_BAD_LBA;
        return 0;
    }

    if (!ata_wait_not_busy()) {
        return 0;
    }

    ata_select_drive(lba);

    port_byte_out(ATA_SECTOR_CNT, 1);
    port_byte_out(ATA_LBA_LOW, (u8)(lba & 0xFF));
    port_byte_out(ATA_LBA_MID, (u8)((lba >> 8) & 0xFF));
    port_byte_out(ATA_LBA_HIGH, (u8)((lba >> 16) & 0xFF));
    port_byte_out(ATA_COMMAND, ATA_CMD_WRITE_SECTORS);

    if (!ata_wait_drq()) {
        return 0;
    }

    for (i = 0; i < 256; i++) {
        word = buffer[i * 2] | ((u16)buffer[i * 2 + 1] << 8);
        port_word_out(ATA_DATA, word);
    }

    port_byte_out(ATA_COMMAND, ATA_CMD_CACHE_FLUSH);

    if (!ata_wait_not_busy()) {
        return 0;
    }

    disk_last_error = DISK_ERROR_NONE;
    return 1;
}

int disk_get_last_error() {
    return disk_last_error;
}

char *disk_get_status_text() {
    if (disk_last_error == DISK_ERROR_NONE) {
        return "OK";
    }

    if (disk_last_error == DISK_ERROR_TIMEOUT) {
        return "TIMEOUT";
    }

    if (disk_last_error == DISK_ERROR_ATA) {
        return "ATA ERROR";
    }

    if (disk_last_error == DISK_ERROR_BAD_LBA) {
        return "BAD LBA";
    }

    return "UNKNOWN";
}