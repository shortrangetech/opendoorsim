#!/bin/bash

# Ensure we're in the right directory (where this script is located)
cd "$(dirname "$0")" || exit 1

# --- Setup PlatformIO ---
PIO_CMD=""
if command -v pio &> /dev/null; then
    PIO_CMD="pio"
elif [ -f "$HOME/.platformio/penv/bin/pio" ]; then
    PIO_CMD="$HOME/.platformio/penv/bin/pio"
else
    echo "PlatformIO not found globally or in VSCode extension path."
    echo "Creating local Python virtual environment (.venv) and installing PlatformIO..."
    if [ ! -d ".venv" ]; then
        python3 -m venv .venv
        source .venv/bin/activate
        pip install -U platformio
        deactivate
    fi
    PIO_CMD="$(pwd)/.venv/bin/pio"
fi

# Extract version from main.cpp
VERSION=$(grep -oE 'String firmwareVersion = "[^"]+"' firmware/src/main.cpp | cut -d'"' -f2)
if [ -z "$VERSION" ]; then
    VERSION="unknown"
fi

# Function to clean up on exit
cleanup() {
    # Reset scrolling region to full terminal
    tput csr 0 $(($(tput lines) - 1))
    # Show cursor
    tput cnorm
    # Move cursor to bottom of screen so prompt doesn't overwrite anything
    tput cup $(($(tput lines) - 1)) 0
    echo -e "\n\033[0;32mFlash script finished.\033[0m"
}
trap cleanup EXIT INT TERM

# Recalculate scrolling region if window is resized
trap 'tput csr 2 $(($(tput lines) - 1))' WINCH

# Clear the terminal
clear

# Hide cursor for a cleaner look
tput civis

LOGO="OPENDOORSIM by shortrange.tech"

# Function to draw the fixed header
draw_header() {
    local progress=$1
    local msg=$2
    local cur_cols=$(tput cols)
    
    # Save cursor position
    tput sc
    
    # Draw line 1: Logo and Version (Blue background)
    tput cup 0 0
    # Calculate spaces to right-align the version
    local spaces=$((cur_cols - ${#LOGO} - ${#VERSION} - 2))
    [ $spaces -lt 0 ] && spaces=0
    local padding=$(printf '%*s' $spaces '')
    echo -ne "\033[44m\033[97m ${LOGO}${padding}${VERSION} \033[0m"
    
    # Draw line 2: Progress bar (Black background)
    tput cup 1 0
    local bar_len=20
    local filled=$(( progress * bar_len / 100 ))
    local empty=$(( bar_len - filled ))
    local bar_filled=$(printf '%*s' $filled '' | tr ' ' '=')
    local bar_empty=$(printf '%*s' $empty '')
    
    local p_str="[${bar_filled}>${bar_empty}] ${progress}% - ${msg}"
    local p_spaces=$((cur_cols - ${#p_str}))
    [ $p_spaces -lt 0 ] && p_spaces=0
    local p_pad=$(printf '%*s' $p_spaces '')
    echo -ne "\033[40m\033[97m${p_str}${p_pad}\033[0m"
    
    # Restore cursor position
    tput rc
}

# Set scrolling region (leave lines 0 and 1 for header, output starts on line 2)
tput csr 2 $(($(tput lines) - 1))
# Move cursor to the start of the scrolling region
tput cup 2 0

run_pio() {
    # We pipe through while read to preserve output and filter out any clear screen 
    # escape sequences that PIO might emit (which destroy the tput csr region).
    # \033c is ESC c (Reset Device)
    # \033[2J is Erase in Display
    "$PIO_CMD" "$@" 2>&1 | while IFS= read -r line; do
        cleaned=$(echo "$line" | sed $'s/\033c//g; s/\033\\[2J//g; s/\033\\[H//g')
        echo "$cleaned"
    done
    return ${PIPESTATUS[0]}
}

draw_header 0 "Starting build process..."
echo "--- Firmware Build Process Initiated Using PIO at: $PIO_CMD ---"

# Step 1: Build Firmware
draw_header 10 "Building Firmware..."
run_pio run -e esp32dev
if [ $? -ne 0 ]; then
    draw_header 10 "Firmware Build Failed!"
    exit 1
fi

# Step 2: Upload Firmware
draw_header 40 "Uploading Firmware..."
run_pio run -t upload -e esp32dev
if [ $? -ne 0 ]; then
    draw_header 40 "Firmware Upload Failed!"
    exit 1
fi

# Step 3: Build Filesystem
draw_header 60 "Building Filesystem..."
run_pio run -t buildfs -e esp32dev
if [ $? -ne 0 ]; then
    draw_header 60 "Filesystem Build Failed!"
    exit 1
fi

# Step 4: Upload Filesystem
draw_header 80 "Uploading Filesystem..."
run_pio run -t uploadfs -e esp32dev
if [ $? -ne 0 ]; then
    draw_header 80 "Filesystem Upload Failed!"
    exit 1
fi

draw_header 100 "Done!"
echo "--- Upload Complete! ---"
