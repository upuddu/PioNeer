#include "sd_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ff.h"      // FatFs library
#include "hw_config.h" // Library header for hw config
#include "config.h"

// Provide stubs for the RP2350 AON timer to satisfy the library's my_rtc.c dependencies
bool aon_timer_is_running(void) { return false; }
void aon_timer_get_time(struct timespec *ts) { }

// ─── Hardware Configuration for no-OS-FatFS-SD-SPI-RPi-Pico ───────────────

static spi_t spi = {
    .hw_inst = SD_SPI_PORT,  // spi0 or spi1
    .miso_gpio = SD_MISO_PIN,
    .mosi_gpio = SD_MOSI_PIN,
    .sck_gpio = SD_CLK_PIN,
    .baud_rate = SD_SPI_BAUD,
};

static sd_spi_if_t spi_if = {
    .spi = &spi,  // Pointer to the SPI driving this card
    .ss_gpio = SD_CS_PIN      // The SPI slave select GPIO for this SD card
};

static sd_card_t sd_card = {
    .type = SD_IF_SPI,
    .spi_if_p = &spi_if,
    .use_card_detect = false,
};

/* ********************************************************************** */
// Required callback functions for the library
size_t sd_get_num() { return 1; }
sd_card_t *sd_get_by_num(size_t num) {
    if (0 == num) {
        return &sd_card;
    } else {
        return NULL;
    }
}

size_t spi_get_num() { return 1; }
spi_t *spi_get_by_num(size_t num) {
    if (0 == num) {
        return &spi;
    } else {
        return NULL;
    }
}

// ─── SD Card API Implementation ───────────────────────────────────────────

// Global FatFs object
static FATFS fs;

bool sd_card_init(void) {
    FRESULT fr;

    // Initialize the SD card SPI interface and mount the file system
    // "0:" is the logical drive number defined in hw_config.c
    fr = f_mount(&fs, "0:", 1);
    
    if (fr != FR_OK) {
        printf("Failed to mount SD card. FatFs Error: %d\n", fr);
        return false;
    }
    
    return true;
}

void sd_card_deinit(void) {
    // Unmount
    f_mount(NULL, "0:", 0);
}

bool sd_card_write_file(const char* filename, const uint8_t* data, size_t length) {
    FIL file;
    FRESULT fr;
    UINT bw; // Bytes written

    // Open file for writing (creates if not exists, overwrites if exists)
    fr = f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        printf("Error opening file %s for write: %d\n", filename, fr);
        return false;
    }

    // Write data
    fr = f_write(&file, data, length, &bw);
    if (fr != FR_OK || bw != length) {
        printf("Error writing to file %s: %d (Bytes written: %u/%u)\n", filename, fr, bw, (unsigned)length);
        f_close(&file);
        return false;
    }

    // Close file
    f_close(&file);
    return true;
}

bool sd_card_read_file(const char* filename, uint8_t* buffer, size_t max_length, size_t* bytes_read) {
    FIL file;
    FRESULT fr;
    UINT br; // Bytes read

    *bytes_read = 0;

    // Open file for reading
    fr = f_open(&file, filename, FA_READ);
    if (fr != FR_OK) {
        printf("Error opening file %s for read: %d\n", filename, fr);
        return false;
    }

    // Read data
    fr = f_read(&file, buffer, max_length, &br);
    if (fr != FR_OK) {
        printf("Error reading from file %s: %d\n", filename, fr);
        f_close(&file);
        return false;
    }

    *bytes_read = br;

    // Close file
    f_close(&file);
    return true;
}

bool sd_card_delete(const char* path) {
    FRESULT fr = f_unlink(path);
    if (fr != FR_OK) {
        printf("Error deleting %s: %d\n", path, fr);
        return false;
    }
    return true;
}

// ─── Directory & File Management ──────────────────────────────────────────

bool sd_card_list_dir(const char* path) {
    DIR dir;
    FILINFO fno;
    FRESULT fr;

    fr = f_opendir(&dir, path);
    if (fr == FR_OK) {
        printf("Directory listing for '%s':\n", path);
        for (;;) {
            fr = f_readdir(&dir, &fno);
            if (fr != FR_OK || fno.fname[0] == 0) break; // Break on error or end of dir
            
            if (fno.fattrib & AM_DIR) {
                printf("  [DIR]  %s\n", fno.fname);
            } else {
                printf("  [FILE] %s\t%llu bytes\n", fno.fname, (unsigned long long)fno.fsize);
            }
        }
        f_closedir(&dir);
        return true;
    } else {
        printf("Failed to open directory '%s'. Error: %d\n", path, fr);
        return false;
    }
}

bool sd_card_get_dir_contents(const char* path, sd_file_info_t** out_list, size_t* out_count) {
    DIR dir;
    FILINFO fno;
    FRESULT fr;
    size_t count = 0;
    size_t capacity = 10;
    
    sd_file_info_t* list = (sd_file_info_t*)malloc(capacity * sizeof(sd_file_info_t));
    if (!list) return false;

    fr = f_opendir(&dir, path);
    if (fr == FR_OK) {
        for (;;) {
            fr = f_readdir(&dir, &fno);
            if (fr != FR_OK || fno.fname[0] == 0) break;
            
            if (count >= capacity) {
                capacity *= 2;
                sd_file_info_t* new_list = (sd_file_info_t*)realloc(list, capacity * sizeof(sd_file_info_t));
                if (!new_list) {
                    free(list);
                    f_closedir(&dir);
                    return false;
                }
                list = new_list;
            }
            
            strncpy(list[count].name, fno.fname, sizeof(list[count].name) - 1);
            list[count].name[sizeof(list[count].name) - 1] = '\0';
            list[count].size = (uint32_t)fno.fsize;
            list[count].is_dir = (fno.fattrib & AM_DIR) != 0;
            count++;
        }
        f_closedir(&dir);
        
        // Shrink the array to the exact size to save memory
        if (count > 0) {
            sd_file_info_t* exact_list = (sd_file_info_t*)realloc(list, count * sizeof(sd_file_info_t));
            if (exact_list) list = exact_list;
        } else {
            free(list);
            list = NULL;
        }
        
        *out_list = list;
        if (out_count) *out_count = count;
        return true;
    } else {
        free(list);
        if (out_count) *out_count = 0;
        return false;
    }
}

void sd_card_free_dir_contents(sd_file_info_t* list) {
    if (list) free(list);
}

bool sd_card_create_dir(const char* path) {
    FRESULT fr = f_mkdir(path);
    if (fr != FR_OK && fr != FR_EXIST) {
        printf("Error creating directory '%s': %d\n", path, fr);
        return false;
    }
    return true;
}

bool sd_card_exists(const char* path) {
    FILINFO fno;
    FRESULT fr = f_stat(path, &fno);
    return fr == FR_OK;
}

// ─── Raw Sector API (DMA Bypassing FatFs) ──────────────────────────────────

bool sd_card_write_sectors(uint32_t start_sector, const uint8_t* buffer, uint32_t sector_count) {
    sd_card_t *sd = sd_get_by_num(0);
    if (!sd || !sd->write_blocks) return false;

    // The underlying driver uses DMA to stream this directly to the SD card
    int err = sd->write_blocks(sd, buffer, start_sector, sector_count);
    return err == 0;
}

bool sd_card_read_sectors(uint32_t start_sector, uint8_t* buffer, uint32_t sector_count) {
    sd_card_t *sd = sd_get_by_num(0);
    if (!sd || !sd->read_blocks) return false;

    // The underlying driver uses DMA to stream this directly into your buffer
    int err = sd->read_blocks(sd, buffer, start_sector, sector_count);
    return err == 0;
}
