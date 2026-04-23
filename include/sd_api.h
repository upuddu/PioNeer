#ifndef SD_API_H
#define SD_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Initialize the SD card interface and mount the file system
bool sd_card_init(void);

// Unmount the file system (useful before sleeping)
void sd_card_deinit(void);

// Write an array of bytes to a file, overwriting existing contents
bool sd_card_write_file(const char* filename, const uint8_t* data, size_t length);

// Read an array of bytes from a file. 
// Populates 'bytes_read' with the actual number of bytes read.
bool sd_card_read_file(const char* filename, uint8_t* buffer, size_t max_length, size_t* bytes_read);

// Delete a file or directory
bool sd_card_delete(const char* path);

// ─── Directory & File Management ──────────────────────────────────────────

// List the contents of a directory (prints to stdout)
bool sd_card_list_dir(const char* path);

typedef struct {
    char name[256];     // File or directory name
    uint32_t size;      // File size in bytes (0 for directories)
    bool is_dir;        // True if it's a directory, false if it's a file
} sd_file_info_t;

// Returns true on success, false on error.
// The function dynamically allocates 'out_list' to fit all files (no maximum cap) and stores the count in 'out_count'.
// You MUST call sd_card_free_dir_contents on 'out_list' when you are done to avoid memory leaks.
bool sd_card_get_dir_contents(const char* path, sd_file_info_t** out_list, size_t* out_count);

// Frees the memory allocated by sd_card_get_dir_contents
void sd_card_free_dir_contents(sd_file_info_t* list);

// Create a new directory
bool sd_card_create_dir(const char* path);

// Check if a file or directory exists
bool sd_card_exists(const char* path);

// Raw Sector Access (Bypasses FAT file system, uses raw DMA directly)
// Note: Each sector is typically 512 bytes.
bool sd_card_write_sectors(uint32_t start_sector, const uint8_t* buffer, uint32_t sector_count);
bool sd_card_read_sectors(uint32_t start_sector, uint8_t* buffer, uint32_t sector_count);

#endif // SD_API_H
