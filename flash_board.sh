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

# --- Settings file paths ---
SETTINGS_FILE="firmware/data/settings.json"
SETTINGS_BAK="firmware/data/settings.json.bak"
SETTINGS_TMP="firmware/data/settings.json.tmp"

# --- Parse flags ---
ADVANCED_MODE=false
while getopts "a" opt; do
    case $opt in
        a) ADVANCED_MODE=true ;;
        *) echo "Usage: $0 [-a]"; exit 1 ;;
    esac
done

# Function to clean up on exit
cleanup() {
    # Restore settings.json if it was swapped out by advanced mode
    if [ -f "$SETTINGS_BAK" ]; then
        mv "$SETTINGS_BAK" "$SETTINGS_FILE"
    fi
    [ -f "$SETTINGS_TMP" ] && rm -f "$SETTINGS_TMP"

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

# --- Advanced Mode: Questionnaire ---
run_questionnaire() {
    # Show cursor for input
    tput cnorm

    local cur_cols=$(tput cols)
    local adv_label=" ADVANCED MODE \u2014 Board Configuration "
    local adv_spaces=$(( (cur_cols - ${#adv_label}) / 2 ))
    [ $adv_spaces -lt 0 ] && adv_spaces=0
    local adv_pad=$(printf '%*s' $adv_spaces '')

    tput cup 2 0
    echo -e "${adv_pad}\033[44m\033[97m${adv_label}\033[0m"
    echo ""

    # 1. SSID
    read -p "  1. WiFi SSID          [OpenDoorSim]: " INPUT_SSID
    INPUT_SSID="${INPUT_SSID:-OpenDoorSim}"

    # 2. Password
    read -p "  2. WiFi Password      [empty=open ]: " INPUT_PWD
    echo ""
    while [ -n "$INPUT_PWD" ] && [ ${#INPUT_PWD} -lt 8 ]; do
        echo "     Password must be 8+ characters, or leave empty for open network."
        read -p "  2. WiFi Password      [empty=open ]: " INPUT_PWD
        echo ""
    done

    # 3. TX Power
    echo ""
    echo "  3. TX Power:"
    echo "     1) Minimum  (-1 dBm)  -- same desk only"
    echo "     2) Very Low  (2 dBm)  -- arm's reach       [default]"
    echo "     3) Low       (5 dBm)  -- same table"
    echo "     4) Medium  (8.5 dBm)  -- across the room"
    echo "     5) High     (13 dBm)  -- full room"
    read -p "     Choose [1-5, default 2]: " INPUT_POWER_CHOICE
    INPUT_POWER_CHOICE="${INPUT_POWER_CHOICE:-2}"
    while ! [[ "$INPUT_POWER_CHOICE" =~ ^[1-5]$ ]]; do
        read -p "     Invalid. Choose [1-5]: " INPUT_POWER_CHOICE
        INPUT_POWER_CHOICE="${INPUT_POWER_CHOICE:-2}"
    done
    # Map 1-5 menu choice to 0-4 index stored in settings.json
    INPUT_TX_POWER=$(( INPUT_POWER_CHOICE - 1 ))

    # 4. Channel
    echo ""
    echo "  4. WiFi Channel (non-overlapping 2.4 GHz):"
    echo "     1) Channel  1  [default]"
    echo "     2) Channel  6"
    echo "     3) Channel 11"
    read -p "     Choose [1-3, default 1]: " INPUT_CHANNEL_CHOICE
    INPUT_CHANNEL_CHOICE="${INPUT_CHANNEL_CHOICE:-1}"
    while ! [[ "$INPUT_CHANNEL_CHOICE" =~ ^[1-3]$ ]]; do
        read -p "     Invalid. Choose [1-3]: " INPUT_CHANNEL_CHOICE
        INPUT_CHANNEL_CHOICE="${INPUT_CHANNEL_CHOICE:-1}"
    done
    case $INPUT_CHANNEL_CHOICE in
        1) INPUT_CHANNEL=1  ;;
        2) INPUT_CHANNEL=6  ;;
        3) INPUT_CHANNEL=11 ;;
    esac

    # Summary
    echo ""
    echo -e "\033[0;32m  Configuration Summary:\033[0m"
    echo "    SSID:     $INPUT_SSID"
    if [ -z "$INPUT_PWD" ]; then
        echo "    Password: (open network)"
    else
        echo "    Password: $INPUT_PWD"
    fi
    echo "    TX Power: $INPUT_POWER_CHOICE/5"
    echo "    Channel:  $INPUT_CHANNEL"
    echo ""
    read -p "  Proceed with flash? [Y/n]: " CONFIRM
    CONFIRM="${CONFIRM:-Y}"
    if [[ "$CONFIRM" =~ ^[Nn] ]]; then
        echo "Aborted."
        exit 0
    fi

    # Hide cursor again before flash output begins
    tput civis
}

# --- Write patched settings.json.tmp and swap it into place ---
write_temp_settings() {
    python3 - <<PYEOF
import json, sys

with open("$SETTINGS_FILE", "r") as f:
    settings = json.load(f)

settings["ap_ssid"]     = "$INPUT_SSID"
settings["ap_pwd"]      = "$INPUT_PWD"
settings["ap_tx_power"] = $INPUT_TX_POWER
settings["ap_channel"]  = $INPUT_CHANNEL

with open("$SETTINGS_TMP", "w") as f:
    json.dump(settings, f, indent=4)
PYEOF

    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to write temporary settings file."
        exit 1
    fi

    # Swap: save original, put patched version in place for the filesystem upload
    cp "$SETTINGS_FILE" "$SETTINGS_BAK"
    cp "$SETTINGS_TMP" "$SETTINGS_FILE"
}

# --- Run questionnaire and prepare settings if advanced mode ---
if [ "$ADVANCED_MODE" = true ]; then
    run_questionnaire
    write_temp_settings
    draw_header 0 "Advanced Mode -- Starting build..."
else
    draw_header 0 "Starting build process..."
fi

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
