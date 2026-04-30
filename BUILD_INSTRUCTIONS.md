# Build & Test Instructions for RP2350A Migration

## Quick Start

### 1. Build the Project
```bash
cd /Users/mtuan1812/Git/PioNeer

# Clean build (optional but recommended first time)
pio run -t clean -e rp2350a_feather

# Build
pio run -e rp2350a_feather
```

### 2. Flash to RP2350A Feather

#### USB Bootloader Method (Easiest)
```bash
# 1. Hold BOOTSEL button, press RESET, release BOOTSEL → USB drive appears
# 2. Run:
pio run -t upload -e rp2350a_feather

# The board auto-resets and runs your code
```

#### Serial UART Method
```bash
# 1. Connect GPIO0 (RX) and GPIO1 (TX) to serial adapter
# 2. Hold BOOTSEL during the upload
pio run -t upload -e rp2350a_feather
```

### 3. Monitor Serial Output
```bash
pio device monitor -e rp2350a_feather --baud 115200
```

---

## Compilation Errors & Fixes

### Error: Board "adafruit_feather_rp2350" not found
**Solution**: Ensure PlatformIO has the latest platform-raspberrypi installed:
```bash
pio platform update raspberrypi
```

### Error: I2C includes not found
**Solution**: Make sure pico-sdk is properly installed:
```bash
pio run -t update --core-packages
```

### Error: LVGL includes missing
**Solution**: LVGL should be in lib_deps. Verify:
```bash
pio run -t download
```

---

## Hardware Verification Checklist

Before flashing, verify:

- [ ] **Power**: RP2350A Feather powered via USB-C
- [ ] **I2C Connection**: Adafruit 5743 connected to STEMMA QT on RP2350A
  - [ ] GPIO2 (SDA) connected
  - [ ] GPIO3 (SCL) connected
  - [ ] GND connected
  - [ ] 3.3V connected

- [ ] **LED Strip**: GPIO21 connected to WS2812B DIN
  - [ ] Data line: GPIO21 → DIN
  - [ ] GND → GND
  - [ ] 5V power connected separately

- [ ] **Display**: HSTX 22-pin breakout connected
  - [ ] All differential pairs connected (clock, D0-D2)
  - [ ] GND and 3.3V connected
  - [ ] DVI/HDMI monitor connected

---

## Expected Serial Output After Boot

```
[BOOT] ===
[BOOT] === PioNeer ===
[INIT] Buttons...
[JOYSTICK] Initialized I2C on GPIO2 (SDA) and GPIO3 (SCL) at 400000Hz
[JOYSTICK] Adafruit 5743 I2C address: 0x50
[INIT] Joystick...
[INIT] Display...
[DISPLAY] Initializing HSTX DVI/HDMI display...
[DISPLAY] HSTX Display Ready (640x480)
[DISPLAY] Frame buffer: 1228800 bytes
[INIT] Menu...
[MAIN] Running menu.
```

---

## Component Testing

### Test I2C Joystick
Add this debug code to `main.c`:
```c
while (true) {
    JoystickReading joy = joystick_read();
    printf("X: %04X, Y: %04X\n", joy.x, joy.y);
    
    if (joystick_sw_consume()) {
        printf("Button pressed!\n");
    }
    
    sleep_ms(100);
}
```

Expected output:
```
X: 7FFF, Y: 8000    (center ~32k range)
X: 0000, Y: 8000    (left)
X: FFFF, Y: 8000    (right)
X: 7FFF, Y: 0000    (up)
X: 7FFF, Y: FFFF    (down)
Button pressed!      (when select button pressed)
```

### Test LED Strip
The startup code in `self_test.c` will test LEDs:
- Red, Green, Blue flash
- Rainbow cycle
- Brightness fade in/out
- Various patterns

Expected: All 10 WS2812B LEDs light up in sequence

### Test Display (HSTX)
The display should show on your connected monitor at 640x480 resolution.
Frame buffer is initialized; actual UI rendering depends on `ui.c` implementation.

---

## Possible Issues & Debug Steps

### Issue: I2C timeout
```bash
# Verify I2C communication
pio device list

# Manual I2C test (if you have i2c-tools):
i2cdetect -y 1    # Check for device at address 0x50
```

### Issue: Display not showing
1. Check HSTX cable connections
2. Try different resolution support on monitor
3. Verify 3.3V power to breakout board

### Issue: LED strip not lighting
1. Check GPIO21 continuity to WS2812B DIN
2. Verify 5V power supply (WS2812B needs 5V, not 3.3V)
3. Test with oscilloscope on GPIO21

### Issue: Compilation fails due to missing headers
```bash
# Regenerate project dependencies
pio project clean
pio run -t download
pio run -e rp2350a_feather
```

---

## Useful Commands

```bash
# List available ports
pio device list

# Clean project
pio run -t clean -e rp2350a_feather

# Build verbose
pio run -v -e rp2350a_feather

# Monitor with timestamps
pio device monitor --timestamp --baud 115200

# Build specific file
pio run -t verbose_build -- src/i2c_joystick.c

# Check board info
pio boards adafruit_feather_rp2350
```

---

## What's Changed

| File | Change |
|------|--------|
| `platformio.ini` | Environment changed to `rp2350a_feather` |
| `include/config.h` | New I2C/HSTX pins, GPIO21 for LED |
| `src/i2c_joystick.c` | NEW - I2C-based joystick driver |
| `src/spi_display.c` | Migrated to HSTX driver |
| `src/led_strip.c` | GPIO9 → GPIO21 |
| `src/self_test.c` | Display init uses HSTX |
| `src/hstx_display.c` | NEW - HSTX support file |

---

## Next Steps for Full Integration

1. **Update `gpio_buttons.c`**: Map buttons from I2C joystick instead of GPIO
2. **Update `ui.c`**: Ensure UI renders properly on 640x480 display
3. **Test game modes**: Verify each game (Lunar Lander, Car Racing, Tank Battle)
4. **Optimize display timing**: Fine-tune HSTX video output if needed
5. **Add error handling**: Graceful fallback if I2C communication fails

---

**Migration Date**: April 29, 2026
**Target Board**: Adafruit RP2350A Feather (Part #6000)
**Status**: Core drivers updated, ready for testing
