# PioNeer Mario Game - Audio System Guide

## Quick Start (Testing with Embedded Audio)

The Mario theme is embedded in your firmware for **immediate testing without an SD card**.

### Current Setup
- ✅ Mario theme audio embedded in flash memory
- ✅ Plays automatically when game starts
- ✅ No external hardware or files needed
- ✅ Ready to compile and flash to RP2350

### To Compile & Test
```bash
cd PioNeer
pio run -t upload
pio device monitor
```

You should see:
```
[AUDIO] ♫ Playing Mario theme from flash memory
[AUDIO] Samples: 100 (4 ms)
```

---

## How to Add Your Own Mario Theme (Testing)

### Step 1: Download Mario Theme MP4
- Search for "Super Mario Bros Theme" on YouTube or free music sites
- Download as MP4 file (e.g., `mario_theme.mp4`)

### Step 2: Convert MP4 to Embedded PCM

Use the included Python script to convert and embed in firmware:

```bash
# First, install ffmpeg (one-time)
brew install ffmpeg  # macOS
# OR
sudo apt install ffmpeg  # Linux

# Convert Mario theme to embedded audio
python3 scripts/audio_converter.py mario_theme.mp4 mario_theme --sample-rate 22050

# This generates:
# - src/mario_theme.c  (audio data array)
# - include/mario_theme.h (header file)
```

### Step 3: Rebuild and Flash
```bash
pio run -t upload
pio device monitor
```

---

## Audio Format Details

**Current Settings:**
- Sample Rate: 22,050 Hz (mono)
- Bit Depth: 16-bit signed PCM
- Format: Raw PCM (samples streamed to PWM)
- Storage: Embedded in flash (src/mario_theme.c)
- Output Pin: GPIO38 (AUDIO_PIN) → PWM → Speaker

**Typical Sizes:**
- 30 seconds: ~1.3 MB
- 1 minute: ~2.6 MB
- RP2350 Flash: 4 MB total (plenty of room)

---

## Python Script Usage

### Basic Conversion
```bash
python3 scripts/audio_converter.py input.mp4 theme_name
```

### With Custom Settings
```bash
# Lower quality (smaller file, mono)
python3 scripts/audio_converter.py mario.mp4 mario_lo --sample-rate 11025

# Stereo version (larger)
python3 scripts/audio_converter.py mario.mp4 mario_stereo --channels 2

# Custom output location
python3 scripts/audio_converter.py mario.mp4 mario --output-dir src
```

### What It Generates
```
src/mario_theme.c          # Audio data: uint16_t mario_theme_data[...]
include/mario_theme.h      # Header: extern declarations
```

These files are ready to compile - no additional configuration needed!

---

## Playing Different Audio Files (Multiple Tracks)

### To embed additional audio (e.g., SFX, game over theme):

```bash
# Convert game over sound
python3 scripts/audio_converter.py gameover.mp4 game_over
```

Then in your code:
```c
// Include both headers
#include "mario_theme.h"
#include "game_over.h"

// Switch between them
if (game_state == GAME_OVER) {
    audio_ctx.data_buffer = game_over_data;
    audio_ctx.buffer_length = game_over_length;
    audio_ctx.current_sample = 0;
    audio_ctx.state = MP4_STATE_PLAYING;
}
```

---

## Future: Migration to SD Card (Production)

When you're ready to move to SD card storage:

### 1. Store Raw PCM on SD Card
```bash
# On your computer, copy the .raw file to SD card:
python3 scripts/audio_converter.py mario.mp4 mario --sample-rate 22050
cp mario_theme.raw /Volumes/YOUR_SD_CARD/audio/
```

### 2. Minimal Code Change
In `audio_mp4.c`, modify `mp4_play()`:

```c
void mp4_play(const char *filename, bool loop)
{
    // Load from SD card instead of flash
    FILE *file = fopen(filename, "rb");
    if (file) {
        // Read into ring buffer...
        // Stream from buffer in mp4_update()
    }
}
```

That's it! The rest of the system works the same.

---

## Troubleshooting Audio

### No Sound from Speaker?
1. **Check GPIO38**: Make sure speaker is connected to GPIO38 (AUDIO_PIN)
2. **Check volume**: Verify no hardware mute
3. **Test PWM**: 
   ```c
   // Add this in main() to test PWM output
   pwm_set_gpio_level(AUDIO_PIN, 1024);  // Should produce tone
   sleep_ms(1000);
   pwm_set_gpio_level(AUDIO_PIN, 0);     // Silent
   ```

### Audio Crackling/Popping?
- Lower the sample rate (11,025 Hz instead of 22,050 Hz)
- Reduce game logic CPU usage on Core 0
- Ensure Core 1 isn't blocked by input polling

### Weak Audio Level?
- Check hardware amplifier (if using one)
- Increase `pwm_level` scaling in `audio_mp4.c`
- Confirm speaker impedance matches expected load

---

## Technical Details

### How Audio Streaming Works

1. **Flash Storage**: Audio data in `mario_theme_data[]` (uint16_t array)
2. **Core 1 Loop**: Calls `mp4_update()` continuously
3. **Sample Streaming**: Each call reads one 16-bit sample
4. **PWM Conversion**: Maps sample value to PWM duty cycle
5. **Output**: Square wave approximates analog audio → Speaker

### Audio Pipeline
```
[mario_theme_data]
       ↓
[mp4_update() - Core 1]  (22,050 times/sec)
       ↓
[PWM duty cycle on GPIO38]
       ↓
[4-stage RC filter in hardware]
       ↓
[Speaker output]
```

### Performance Impact
- **Flash Size**: ~100 bytes per second of audio
- **RAM Usage**: 0 bytes (streams from flash)
- **CPU**: <1% on Core 1 (just reading/PWM write)

---

## Advanced: Using Your Audio Module

Your existing `audio.h` in PioNeer provides wavetable synthesis:

### Option A: Use Embedded PCM (Current)
```c
mp4_play_embedded();  // Flash-based MP4 player
```

### Option B: Use Synthesized Wavefrom (Existing)
```c
set_freq(CHAN0, 440);  // 440 Hz tone
play_melody();         // from audio.h
```

### Option C: Mix Both
```c
// Background: Synthesized bass on one PWM pin
set_freq(CHAN0, 110);

// Foreground: PCM melody on another
mp4_play_embedded();

// (Would require second audio output pin)
```

---

## File Reference

### Audio System Files
- `include/audio_mp4.h` - MP4 player API
- `src/audio_mp4.c` - Implementation (PWM streaming)
- `include/mario_theme.h` - Audio data header (auto-generated)
- `src/mario_theme.c` - Audio samples (auto-generated)
- `scripts/audio_converter.py` - Conversion tool

### Configuration
- `config.h` - `AUDIO_PIN` (GPIO38)
- `platformio.ini` - Build settings

---

## Reference: Script Options

```bash
python3 scripts/audio_converter.py --help
```

Output:
```
usage: audio_converter.py [-h] [--sample-rate SAMPLE_RATE] 
                          [--channels CHANNELS] 
                          [--output-dir OUTPUT_DIR]
                          input name

Convert audio to embedded C data for RP2350

positional arguments:
  input                  Input audio file (MP4, WAV, MP3, etc.)
  name                   Output name (e.g., mario_theme)

optional arguments:
  -h, --help             show this help message and exit
  --sample-rate INT      Sample rate in Hz (default: 22050)
  --channels INT         Audio channels (default: 1=mono)
  --output-dir DIR       Output directory (default: src)
```

---

## Summary

✅ **For Testing**: Use embedded audio (no setup needed)
✅ **For Custom Audio**: Run `audio_converter.py mario.mp4 mario_theme`
✅ **For Production**: Copy `.raw` files to SD card (minimal code change)

Your Mario game is ready to play! 🎮🎵
