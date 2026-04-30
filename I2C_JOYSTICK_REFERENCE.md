# Adafruit 5743 I2C Joystick Integration Reference

## Device Information
- **Product**: Adafruit 5743 STEMMA QT Joystick with Buttons
- **I2C Address**: 0x50 (default)
- **I2C Bus**: I2C1 on RP2350A Feather
- **Communication**: 400kHz I2C (standard mode)
- **Connector**: STEMMA QT (PH 2.0mm)

---

## Pin Mapping

### RP2350A Feather to Adafruit 5743
| STEMMA QT Pin | Signal | RP2350 GPIO | Description |
|---------------|--------|----------|-------------|
| 1 | GND | GND | Ground |
| 2 | 3.3V | 3.3V | Power supply |
| 3 | SDA | GPIO2 | I2C Data |
| 4 | SCL | GPIO3 | I2C Clock |

### I2C Configuration
```c
#define I2C_PORT        i2c1         // I2C instance
#define I2C_SDA_PIN     2            // GPIO2
#define I2C_SCL_PIN     3            // GPIO3
#define I2C_FREQ        400000       // 400kHz
#define JOYSTICK_I2C_ADDR 0x50       // 7-bit address
```

---

## I2C Register Map

### Data Registers
| Address | Size | Field | Range | Description |
|---------|------|-------|-------|-------------|
| 0x00 | 1 byte | X LSB | 0x00-0xFF | Joystick X-axis (LSB) |
| 0x01 | 1 byte | X MSB | 0x00-0xFF | Joystick X-axis (MSB) |
| 0x02 | 1 byte | Y LSB | 0x00-0xFF | Joystick Y-axis (LSB) |
| 0x03 | 1 byte | Y MSB | 0x00-0xFF | Joystick Y-axis (MSB) |
| 0x04 | 1 byte | BUTTONS | 0x00-0xFF | Button state register |

### Combined 16-bit Values
| Field | Registers | Range | Rest Position |
|-------|-----------|-------|----------------|
| X Axis | 0x00-0x01 | 0x0000 - 0xFFFF | ~0x8000 (32768) |
| Y Axis | 0x02-0x03 | 0x0000 - 0xFFFF | ~0x8000 (32768) |

### Button Register (0x04) Bit Mapping
| Bit | Button | Active State | Notes |
|-----|--------|--------------|-------|
| 7 | SELECT (Press) | LOW (0) | Joystick push button |
| 6 | BUTTON_A | LOW (0) | A button |
| 5 | BUTTON_B | LOW (0) | B button |
| 4 | BUTTON_X | LOW (0) | X button |
| 3 | BUTTON_Y | LOW (0) | Y button |
| 2:0 | Reserved | - | Unused |

**Note**: Buttons are active-LOW (0 = pressed, 1 = released)

---

## I2C Read Protocol

### Reading 16-bit Joystick Value
```
1. Send START condition
2. Send address + READ (0x50 << 1 | 1 = 0xA1)
3. Send register address (0x00 for X, 0x02 for Y)
4. Read 2 bytes (LSB first, then MSB)
   - First byte = LSB (0x00-0x01 for X or 0x02-0x03 for Y)
   - Second byte = MSB
5. Send STOP condition
6. Combine: value = (msb << 8) | lsb
```

### Reading Button State
```
1. Send START condition
2. Send address + READ (0xA1)
3. Send register address (0x04)
4. Read 1 byte
5. Send STOP condition
6. Parse: bit 7 = SELECT, bits 6-3 = A/B/X/Y
```

---

## C Implementation Reference

### I2C Initialization
```c
void joystick_init(void)
{
    i2c_init(I2C_PORT, I2C_FREQ);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
}
```

### Reading X-Axis (16-bit)
```c
uint16_t read_x_axis(void)
{
    uint8_t buffer[2];
    uint8_t reg = 0x00;  // X register address
    
    i2c_write_blocking(I2C_PORT, JOYSTICK_I2C_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, JOYSTICK_I2C_ADDR, buffer, 2, false);
    
    return ((uint16_t)buffer[1] << 8) | buffer[0];
}
```

### Reading Button State
```c
uint8_t read_buttons(void)
{
    uint8_t reg = 0x04;   // Button register
    uint8_t buttons = 0xFF;
    
    i2c_write_blocking(I2C_PORT, JOYSTICK_I2C_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, JOYSTICK_I2C_ADDR, &buttons, 1, false);
    
    return buttons;
}
```

### Parsing Button Presses
```c
typedef struct {
    bool select;
    bool button_a;
    bool button_b;
    bool button_x;
    bool button_y;
} ButtonState;

ButtonState parse_buttons(uint8_t reg_value)
{
    ButtonState state;
    state.select   = !(reg_value & (1 << 7));  // Bit 7, active-low
    state.button_a = !(reg_value & (1 << 6));  // Bit 6, active-low
    state.button_b = !(reg_value & (1 << 5));  // Bit 5, active-low
    state.button_x = !(reg_value & (1 << 4));  // Bit 4, active-low
    state.button_y = !(reg_value & (1 << 3));  // Bit 3, active-low
    return state;
}
```

---

## Testing & Verification

### Verify I2C Communication
```bash
# Using i2c-tools (if installed)
i2cdetect -y 1
# Output should show device at address 50 (hex)
```

### Debug Output Example
```c
printf("I2C Test:\n");
uint16_t x = read_x_axis();
uint16_t y = read_y_axis();
uint8_t buttons = read_buttons();

printf("X: 0x%04X (%d)\n", x, x);
printf("Y: 0x%04X (%d)\n", y, y);
printf("Buttons: 0x%02X\n", buttons);

// Parse and display button states
ButtonState state = parse_buttons(buttons);
printf("Select: %s, A: %s, B: %s, X: %s, Y: %s\n",
    state.select ? "PRESSED" : "released",
    state.button_a ? "PRESSED" : "released",
    state.button_b ? "PRESSED" : "released",
    state.button_x ? "PRESSED" : "released",
    state.button_y ? "PRESSED" : "released");
```

---

## Common Issues & Solutions

### Issue: I2C ACK not received
**Symptoms**: `PICO_ERROR_GENERIC` on i2c_write_blocking
**Causes**:
- Device not powered (check 3.3V on STEMMA QT)
- SDA/SCL not connected
- Pull-up resistors not active
- Wrong I2C address

**Solution**:
1. Verify power and connections
2. Check GPIO2/GPIO3 are not used by other peripherals
3. Test with `i2cdetect -y 1` (if available)
4. Add debug: `printf("I2C init on GPIO%d, %d\n", I2C_SDA_PIN, I2C_SCL_PIN);`

### Issue: Joystick reads 0x0000 or 0xFFFF
**Symptoms**: Always reading extreme values
**Causes**:
- Wrong register address
- Byte order reversed
- I2C communication failure

**Solution**:
1. Verify register addresses: X=0x00/0x01, Y=0x02/0x03, Buttons=0x04
2. Check byte order: LSB comes first, then MSB
3. Add retry logic with small delays between attempts

### Issue: Buttons always report as "released"
**Symptoms**: Button register reads all 1s (0xFF)
**Causes**:
- Button logic inverted
- Register not being read correctly
- Active-LOW interpretation error

**Solution**:
1. Remember: 0 = pressed, 1 = released
2. Use `!(reg & (1 << bit))` for parsing
3. Test with oscilloscope on SCL/SDA to verify I2C transactions

---

## Timeout Handling

The driver includes retry logic for I2C reliability:
```c
#define I2C_READ_RETRIES 3

for (int retry = 0; retry < I2C_READ_RETRIES; retry++)
{
    int result = i2c_write_blocking(I2C_PORT, JOYSTICK_I2C_ADDR, &reg, 1, true);
    if (result != PICO_ERROR_GENERIC) break;
    sleep_ms(10);
}
```

---

## Performance Notes

- **I2C Speed**: 400kHz (standard mode)
- **Read Latency**: ~1-2ms per 16-bit value
- **Update Rate**: Typically read every 10-50ms in game loop
- **Power Consumption**: <1mA for joystick + I2C bus

---

## References

- Adafruit 5743 Documentation: https://learn.adafruit.com/
- RP2350 I2C Datasheet: https://datasheets.raspberrypi.com/rp2350/
- Pico SDK I2C Docs: https://github.com/raspberrypi/pico-sdk

---

**Last Updated**: April 29, 2026
**Compatibility**: RP2350A Feather + Adafruit 5743
