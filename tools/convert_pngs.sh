#!/bin/bash
# Convert PNG images to 8-bit indexed format for PSVita compatibility
# This fixes error 0x8010113D during VPK installation

set -e

echo "Converting PNGs to 8-bit indexed format for PSVita..."

# Ensure ImageMagick is available
if ! command -v convert &> /dev/null; then
    echo "Error: ImageMagick 'convert' command not found"
    echo "Please install ImageMagick: sudo apt-get install imagemagick"
    exit 1
fi

# Convert icon
echo "Converting sce_sys/icon0.png..."
convert sce_sys/icon0.png -colors 256 -type Palette sce_sys/icon0.png

# Convert background
echo "Converting sce_sys/livearea/contents/bg.png..."
convert sce_sys/livearea/contents/bg.png -colors 256 -type Palette sce_sys/livearea/contents/bg.png

# Convert startup screen
echo "Converting sce_sys/livearea/contents/startup.png..."
convert sce_sys/livearea/contents/startup.png -colors 256 -type Palette sce_sys/livearea/contents/startup.png

echo "✓ All PNGs converted to 8-bit indexed format"
echo "  The VPK should now install without error 0x8010113D"
