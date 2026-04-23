#!/bin/bash
# Quick audio conversion for PioNeer Mario Game
# Usage: ./quick_convert.sh mario_theme.mp4

if [ $# -eq 0 ]; then
    echo "Usage: $0 <audio_file.mp4>"
    echo ""
    echo "Examples:"
    echo "  $0 mario_theme.mp4          # Creates mario_theme in src/"
    echo "  $0 my_song.wav              # Creates my_song in src/"
    echo ""
    exit 1
fi

INPUT_FILE="$1"
FILENAME=$(basename "$INPUT_FILE")
NAME="${FILENAME%.*}"  # Remove extension

echo "🎵 Converting $INPUT_FILE → $NAME"
echo ""

# Check if ffmpeg is installed
if ! command -v ffmpeg &> /dev/null; then
    echo "❌ ffmpeg not found!"
    echo ""
    echo "Install it first:"
    echo "  macOS:  brew install ffmpeg"
    echo "  Linux:  sudo apt install ffmpeg"
    exit 1
fi

# Run the Python converter
python3 scripts/audio_converter.py "$INPUT_FILE" "$NAME"

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Conversion complete!"
    echo ""
    echo "Files created:"
    echo "  - src/${NAME}.c"
    echo "  - include/${NAME}.h"
    echo ""
    echo "Next: pio run -t upload"
else
    echo ""
    echo "❌ Conversion failed"
    exit 1
fi
