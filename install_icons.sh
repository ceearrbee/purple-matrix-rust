#!/bin/bash

# Define the base directory for local Pidgin protocol icons
BASE_DIR="$HOME/.local/share/pixmaps/pidgin/protocols"

echo "Installing Matrix icons to $BASE_DIR..."

# Loop through sizes and install
for size in 16 22 48; do
    TARGET_DIR="$BASE_DIR/$size"
    
    # Create directory if it doesn't exist
    mkdir -p "$TARGET_DIR"
    
    # Copy the icon
    cp "icons/$size/matrix.png" "$TARGET_DIR/matrix.png"
    
    echo "Installed icons/$size/matrix.png -> $TARGET_DIR/matrix.png"
done

echo "Icon installation complete. You may need to restart Pidgin."
