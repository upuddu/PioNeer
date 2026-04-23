#!/usr/bin/env python3
"""
Audio converter for embedding audio in RP2350 firmware.
Converts MP4/WAV/MP3 to raw PCM and generates C header for embedding.

Usage:
    python3 audio_converter.py input.mp4 mario_theme --sample-rate 22050 --channels 1

This generates:
    - mario_theme.c (audio data array)
    - mario_theme.h (audio header with length)
"""

import sys
import os
import subprocess
import struct
import argparse
from pathlib import Path

def convert_to_pcm(input_file, output_pcm, sample_rate=22050, channels=1):
    """Convert audio file to raw PCM using ffmpeg"""
    print(f"[AUDIO] Converting {input_file} to {sample_rate}Hz mono PCM...")
    
    cmd = [
        'ffmpeg',
        '-i', input_file,
        '-f', 's16le',           # 16-bit signed integer little-endian
        '-acodec', 'pcm_s16le',  # PCM codec
        '-ar', str(sample_rate), # Sample rate
        '-ac', str(channels),    # Audio channels (1=mono)
        '-loglevel', 'error',    # Suppress verbose output
        output_pcm
    ]
    
    try:
        subprocess.run(cmd, check=True, capture_output=True)
        print(f"[AUDIO] ✓ Converted successfully")
        return True
    except subprocess.CalledProcessError as e:
        print(f"[ERROR] FFmpeg conversion failed: {e.stderr.decode()}")
        print("[INFO] Install ffmpeg: brew install ffmpeg (macOS) or apt install ffmpeg (Linux)")
        return False
    except FileNotFoundError:
        print("[ERROR] ffmpeg not found! Install it first:")
        print("  macOS: brew install ffmpeg")
        print("  Linux: sudo apt install ffmpeg")
        return False

def pcm_to_c_header(pcm_file, output_base, name):
    """Convert PCM data to C arrays"""
    with open(pcm_file, 'rb') as f:
        pcm_data = f.read()
    
    print(f"[AUDIO] Audio size: {len(pcm_data)} bytes ({len(pcm_data) / 2} samples @ 16-bit)")
    
    # Create C source file with audio data
    c_content = f'''// AUTO-GENERATED: Do not edit! Use audio_converter.py
// {Path(pcm_file).name} -> {output_base}.c

#include "{output_base}.h"
#include <stdint.h>

// Audio data: PCM 16-bit, {len(pcm_data) // 2} samples
const uint16_t {name}_data[{len(pcm_data) // 2}] = {{
'''
    
    # Write samples as hex (16-bit values)
    samples = struct.unpack(f'<{len(pcm_data)//2}H', pcm_data)
    
    for i, sample in enumerate(samples):
        if i % 16 == 0:  # 16 samples per line
            c_content += '\n    '
        c_content += f'0x{sample:04x}, '
    
    c_content += '\n};\n'
    c_content += f'const uint32_t {name}_length = {len(pcm_data) // 2};\n'
    
    # Create header file
    h_content = f'''// AUTO-GENERATED: Do not edit! Use audio_converter.py
#ifndef {output_base.upper()}_H
#define {output_base.upper()}_H

#include <stdint.h>

extern const uint16_t {name}_data[];
extern const uint32_t {name}_length;

#endif // {output_base.upper()}_H
'''
    
    # Write files
    c_file = f'{output_base}.c'
    h_file = f'{output_base}.h'
    
    with open(c_file, 'w') as f:
        f.write(c_content)
    print(f"[GEN] Created: {c_file}")
    
    with open(h_file, 'w') as f:
        f.write(h_content)
    print(f"[GEN] Created: {h_file}")
    
    return len(pcm_data) // 2

def main():
    parser = argparse.ArgumentParser(
        description='Convert audio to embedded C data for RP2350'
    )
    parser.add_argument('input', help='Input audio file (MP4, WAV, MP3, etc.)')
    parser.add_argument('name', help='Output name (e.g., mario_theme)')
    parser.add_argument('--sample-rate', type=int, default=22050,
                        help='Sample rate in Hz (default: 22050)')
    parser.add_argument('--channels', type=int, default=1,
                        help='Audio channels (default: 1=mono)')
    parser.add_argument('--output-dir', default='src',
                        help='Output directory (default: src)')
    
    args = parser.parse_args()
    
    # Check input file exists
    if not os.path.exists(args.input):
        print(f"[ERROR] Input file not found: {args.input}")
        sys.exit(1)
    
    # Create temp PCM file
    pcm_temp = '/tmp/audio_temp.pcm'
    
    # Convert to PCM
    if not convert_to_pcm(args.input, pcm_temp, args.sample_rate, args.channels):
        sys.exit(1)
    
    # Ensure output directory exists
    os.makedirs(args.output_dir, exist_ok=True)
    output_base = os.path.join(args.output_dir, args.name)
    
    # Convert PCM to C
    sample_count = pcm_to_c_header(pcm_temp, output_base, args.name)
    
    # Cleanup
    os.remove(pcm_temp)
    
    print(f"\n[SUCCESS] Audio embedded!")
    print(f"[INFO] Include in your code:")
    print(f"       #include \"mario_theme.h\"")
    print(f"       Then use: mario_theme_data (pointer)")
    print(f"                 mario_theme_length (sample count)")
    print(f"\n[INFO] Total size: {sample_count * 2 / 1024:.1f} KB")

if __name__ == '__main__':
    main()
