# RP2350A Feather Hardware Migration Guide

## Overview
This project has been migrated from the **Proton board** to the **Adafruit RP2350A Feather** with the following hardware changes:

| Component | Old | New |
|-----------|-----|-----|
| **Microcontroller** | Proton (custom RP2040) | RP2350A Feather (part #6000) |
| **Input** | GPIO analog joystick + GPIO buttons | Adafruit 5743 (I2C) |
| **Display** | SPI TFT LCD (480x320) | HSTX DVI/HDMI 22-pin breakout (640x480) |
| **LED Strip** | GPIO9 | GPIO21 |
| **Flash Method** | SWD debugger (picoprobe) | USB bootloader (UF2) |

---

## Updated Configuration

### `platformio.ini`
- **Board**: `adafruit_feather_rp2350`
- **Platform**: `raspberrypi` (standard)
- **Upload Protocol**: `rp2040-uf2` (USB bootloader)

### `include/config.h` - Pin Assignments

#### I2C Joystick (Adafruit 5743)
```c
#define I2C_PORT        i2c1
#define I2C_SDA_PIN     2        // GPIO2 (STEMMA QT connector)
#define I2C_SCL_PIN     3        // GPIO3 (STEMMA QT connector)
#define I2C_FREQ        400000   // 400kHz standard I2C
#define JOYSTICK_I2C_ADDR 0x50   // Adafruit 5743 default address
```

**Note**: The Adafruit RP2350A Feather has a **STEMMA QT connector** on I2C1 (GPIO2/GPIO3). The Adafruit 5743 connects directly via this connector.

#### HSTX Display (DVI/HDMI)
```c
#define HSTX_ENABLED    1
#define LCD_WIDTH       640
#define LCD_HEIGHT      480
```

**Note**: HSTX uses dedicated pins on the RP2350 (GPIO 0-23 are reserved for video output). No explicit pin configuration needed.

#### LED Strip (WS2812B)
```c
#define WS2812_PIN      21
#define WS2812_NUM_LEDS 10
```

---

## Hardware Connections

### Adafruit 5743 (Joystick + Buttons)
| Connector | Pin | RP2350A Feather |
|-----------|-----|-----------------|
| **STEMMA QT** | GND | GND |
| **STEMMA QT** | 3V3 | 3V3 |
| **STEMMA QT** | SDA | GPIO2 |
| **STEMMA QT** | SCL | GPIO3 |

The Adafruit 5743 provides:
- **4 Buttons**: A, B, X, Y (mapped from I2C register 0x04)
- **Analog Joystick**: X and Y axes (16-bit, registers 0x00-0x03)
- **Select Button**: Available on I2C

### HSTX DVI/HDMI Breakout (22-pin)
| Signal | RP2350 Pin | Breakout Pin |
|--------|-----------|--------------|
| Clock+ | GPIO0 | Clock P |
| Clock- | GPIO1 | Clock N |
| D0 Pair+ | GPIO2 | D0 P |
| D0 Pair- | GPIO3 | D0 N |
| D1 Pair+ | GPIO4 | D1 P |
| D1 Pair- | GPIO5 | D1 N |
| D2 Pair+ | GPIO6 | D2 P |
| D2 Pair- | GPIO7 | D2 N |
| GND | GND | GND |
| 3.3V | 3.3V | 3.3V |

**Note**: The HSTX pins are fixed and dedicated on RP2350. Refer to the [RP2350 Datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf) for complete pin mapping.

### WS2812B LED Strip
| Signal | Pin | Connection |
|--------|-----|-----------|
| Data | GPIO21 | DIN |
| GND | GND | GND |
| 5V | VBUS | +5V |

---

## Flashing the Device

### Method 1: USB Bootloader (UF2) — Recommended

1. **Enter Bootloader Mode**:
   - Hold the **BOOTSEL** button on your RP2350A Feather
   - While holding, press and release the **RESET** button (or cycle power)
   - Release BOOTSEL after 1-2 seconds
   - A USB drive named `RPI-RP2` will appear on your computer

2. **Build and Flash**:
   ```bash
   cd /Users/mtuan1812/Git/PioNeer
   pio run -t upload
   ```
   - The compiled `.uf2` file will be automatically copied to the USB drive
   - The board will reboot and run your code

3. **Troubleshooting**:
   - USB drive doesn't appear: Try a different USB cable or repeat bootloader entry
   - Permission denied: Check USB permissions with `ls -la /media/RPI-RP2/`

### Method 2: UART Serial Bootloader

1. **Connect UART Adapter**:
   ```
   RP2350A TX (GPIO1) → RX on serial adapter
   RP2350A RX (GPIO0) → TX on serial adapter
   RP2350A GND → GND on serial adapter
   ```

2. **Configure platformio.ini** (optional for UART):
   ```ini
   upload_protocol = serial
   upload_port = /dev/tty.usbserial-XXXXX  # macOS
   upload_speed = 115200
   ```

3. **Enter Bootloader and Flash**:
   ```bash
   # Hold BOOTSEL during the entire flash process
   pio run -t upload
   ```

4. **Find UART Port**:
   ```bash
   pio device list
   ```

---

## Source Code Changes

### New/Updated Files

#### `src/i2c_joystick.c` (NEW)
- Replaces ADC-based joystick reading
- Communicates with Adafruit 5743 via I2C
- Maintains same API as old `adc_joystick.c`
- Supports button debouncing

#### `src/spi_display.c` (UPDATED)
- Migrated from SPI TFT LCD to HSTX DVI/HDMI
- Uses LVGL display creation API
- Frame buffer allocated in RAM
- Resolution changed to 640x480

#### `src/led_strip.c` (UPDATED)
- GPIO pin changed from GPIO9 to GPIO21
- All LED functions unchanged (uses GPIO21 via `config.h`)

#### `src/self_test.c` (NEEDS UPDATE)
- Currently contains SPI-specific display init code
- Need to replace with HSTX display initialization
- LED tests remain compatible

### Configuration Header
**`include/config.h`** updated with:
- I2C pin definitions
- HSTX display defines
- New GPIO21 for WS2812B
- Removed old SPI and ADC pin definitions

---

## Build & Compilation

### Install Dependencies
```bash
# Ensure you have PlatformIO installed
pip install platformio

# Install PlatformIO core if needed
pio account login
```

### Build the Project
```bash
cd /Users/mtuan1812/Git/PioNeer
pio run -e rp2350a_feather
```

### Clean Build
```bash
pio run -t clean -e rp2350a_feather
pio run -e rp2350a_feather
```

---

## Debugging & Serial Monitor

### Connect Serial Monitor
After flashing, connect the USB cable for serial output (115200 baud):

```bash
pio device monitor -e rp2350a_feather
```

### Expected Boot Output
```
[BOOT] ===
[BOOT] === PioNeer ===
[INIT] Buttons...
[JOYSTICK] Initialized I2C on GPIO2 (SDA) and GPIO3 (SCL) at 400000Hz
[JOYSTICK] Adafruit 5743 I2C address: 0x50
[INIT] Joystick...
[INIT] Display...
[DISPLAY] Initializing HSTX DVI/HDMI breakout (640x480@60Hz)
[DISPLAY] Frame buffer allocated: XXXXX bytes
[DISPLAY] Display initialized: 640x480
[DISPLAY] HSTX DVI/HDMI output active on dedicated pins
[INIT] Menu...
[MAIN] Running menu.
```

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| **I2C not responding** | Check STEMMA QT connector; verify I2C address with `i2cdetect` |
| **Display blank** | Verify HSTX DVI/HDMI cable; check monitor resolution support (640x480) |
| **LED strip not working** | Test GPIO21 with oscilloscope; verify WS2812B power supply (5V) |
| **Build fails** | Run `pio run -t clean` and rebuild; check platform compatibility |
| **Serial output garbled** | Verify baud rate is 115200; check USB cable |
| **Board not detected** | Try different USB port; reset with BOOTSEL again |

---

## Additional Resources

- **RP2350 Datasheet**: https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf
- **Adafruit Feather RP2350A**: https://www.adafruit.com/product/6000
- **Adafruit 5743 Joystick**: https://www.adafruit.com/product/5743
- **PlatformIO Docs**: https://docs.platformio.org/
- **LVGL Documentation**: https://docs.lvgl.io/

---

## Next Steps

1. ✅ Update `platformio.ini` for RP2350A board
2. ✅ Update `config.h` with new pin definitions
3. ✅ Update `led_strip.c` for GPIO21
4. ✅ Create `i2c_joystick.c` for Adafruit 5743
5. ✅ Create HSTX display driver
6. ⚠️ **TODO**: Update `self_test.c` to use HSTX display initialization
7. ⚠️ **TODO**: Verify button mapping from Adafruit 5743
8. ⚠️ **TODO**: Test I2C communication reliability
9. ⚠️ **TODO**: Optimize HSTX video output (timing/resolution)
10. ⚠️ **TODO**: Update `gpio_buttons.c` to read from I2C joystick instead of GPIO

---

**Last Updated**: April 29, 2026
**Status**: Hardware configuration complete, driver implementation in progress
