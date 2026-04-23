#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "sd_api.h"
#include "hardware/timer.h"
#include "hardware/sync.h"
#include "pico/util/datetime.h"
#include "pico/aon_timer.h"

#define TEST_FILE "test.dat"
#define DATA_SIZE (64 * 1024) // 64 KB test file

uint8_t write_buf[DATA_SIZE];
uint8_t read_buf[DATA_SIZE];

static volatile bool timer_fired = false;
static void alarm_irq(uint alarm_num) {
    timer_fired = true;
}

// Puts the RP2350 to sleep until a specific time elapses
void sleep_and_wake(uint32_t ms) {
    timer_fired = false;
    int alarm_num = hardware_alarm_claim_unused(true);
    if (alarm_num < 0) {
        printf("Failed to claim hardware alarm!\n");
        sleep_ms(ms);
        return;
    }
    
    hardware_alarm_set_callback(alarm_num, alarm_irq);
    hardware_alarm_set_target(alarm_num, make_timeout_time_ms(ms));
    printf("Going to sleep for %u ms (WFI)...\n", (unsigned int)ms);
    
    // Sleep loop: CPU sleeps via WFI, wakes up on ANY interrupt (like USB),
    // and goes back to sleep if our timer hasn't fired yet.
    while (!timer_fired) {
        __wfi(); // Wait for Interrupt
    }
    
    printf("Woke up from timer interrupt!\n");
    hardware_alarm_set_callback(alarm_num, NULL);
    hardware_alarm_unclaim(alarm_num);
}

int main(void) {
    stdio_init_all();

    // Give some time for USB serial to connect
    sleep_ms(3000);

    printf("==========================================\n");
    printf(" PioNeer SD Card High-Speed Test Start \n");
    printf("==========================================\n");

    if (!sd_card_init()) {
        printf("SD Card initialization failed.\n");
        while(1) sleep_ms(1000);
    }
    printf("SD Card mounted successfully.\n");

    // Populate write buffer with a predictable pattern
    printf("Generating %u bytes of test data...\n", DATA_SIZE);
    for (size_t i = 0; i < DATA_SIZE; i++) {
        write_buf[i] = (uint8_t)(i & 0xFF);
    }

    // 1. Measure Write Speed
    printf("\nWriting %u bytes to %s...\n", DATA_SIZE, TEST_FILE);
    uint32_t start_us = time_us_32();
    if (sd_card_write_file(TEST_FILE, write_buf, DATA_SIZE)) {
        uint32_t end_us = time_us_32();
        uint32_t duration_us = end_us - start_us;
        float speed_kbps = ((float)DATA_SIZE / 1024.0f) / ((float)duration_us / 1000000.0f);
        printf("Write complete in %u us. Speed: %.2f KB/s\n", (unsigned int)duration_us, speed_kbps);
    } else {
        printf("Write failed!\n");
    }

    // 2. Sleep & Wake Test
    printf("\nUnmounting SD card for sleep...\n");
    sd_card_deinit(); 
    sleep_and_wake(5000); // Sleep for 5 seconds
    printf("Remounting SD card...\n");
    sd_card_init();   

    // 3. Measure Read Speed
    printf("\nReading %u bytes from %s...\n", DATA_SIZE, TEST_FILE);
    size_t bytes_read = 0;
    start_us = time_us_32();
    if (sd_card_read_file(TEST_FILE, read_buf, DATA_SIZE, &bytes_read)) {
        uint32_t end_us = time_us_32();
        uint32_t duration_us = end_us - start_us;
        float speed_kbps = ((float)bytes_read / 1024.0f) / ((float)duration_us / 1000000.0f);
        printf("Read %u bytes complete in %u us. Speed: %.2f KB/s\n", (unsigned int)bytes_read, (unsigned int)duration_us, speed_kbps);
    } else {
        printf("Read failed!\n");
    }

    // 4. Verify Data Integrity
    printf("\nVerifying data integrity...\n");
    bool match = true;
    for (size_t i = 0; i < DATA_SIZE; i++) {
        if (write_buf[i] != read_buf[i]) {
            printf("Mismatch at index %zu: Expected %02X, Got %02X\n", i, write_buf[i], read_buf[i]);
            match = false;
            break;
        }
    }

    if (match) {
        printf("SUCCESS! All data matches perfectly.\n");
    } else {
        printf("FAILURE! Data corruption detected.\n");
    }

    // Cleanup test file
    sd_card_delete(TEST_FILE);
    printf("Test complete. Entering infinite loop.\n");

    while (1) {
        sleep_ms(1000);
    }

    return 0;
}
