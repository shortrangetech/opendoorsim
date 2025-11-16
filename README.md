# DoorSim

This project is a refactored version of the [DoorSim](https://github.com/evildaemond/doorsim) Firmware by evildaemond, designed to work with PlatformIO.

## Key Changes from Original Firmware
HTML, CSS, and JavaScript: Moved to the LittleFS filesystem for easier management and updates.

Preferences and Credentials: Stored in LittleFS instead of using the Preferences library.

PlatformIO: Used as the development environment for easy library management and project configuration.

## Setup Instructions
1. Install PlatformIO
Download and install PlatformIO for your preferred IDE (e.g., VSCode).

2. Clone the Repository
Clone this repository to your local machine:

```bash
git clone git@github.com:nechry/DoorSim.git
```

3. Open the Project in PlatformIO
Open the project folder in VSCode with PlatformIO installed.

PlatformIO will automatically detect the project and install the required dependencies.

4. Upload Files to LittleFS

Use the PlatformIO Upload Filesystem Image tool to upload these files to the ESP32's LittleFS partition:

Click the PlatformIO icon in the left sidebar.

Go to Project Tasks > esp32dev > Platform > Build Filesystem Image.

Once the filesystem image is built, go to Project Tasks > esp32dev > Platform > Upload Filesystem Image.

5. Configure the Project
Ensure the platformio.ini file is configured correctly for your ESP32 board.

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
board_build.filesystem = littlefs
lib_deps = 
	bblanchon/ArduinoJson@^7.3.0
	me-no-dev/AsyncTCP@^3.3.2
	me-no-dev/ESPAsyncWebServer@^3.6.0
	iakop/LiquidCrystal_I2C_ESP32@^1.1.6
monitor_speed = 115200
```

6. Upload the Code
Connect your ESP32 to your computer via USB.

Click the Upload button in PlatformIO (or run pio run --target upload in the terminal).

7. Configure Settings
After uploading the code, the ESP32 will start in Access Point (AP) mode with the SSID doorsim.

Connect to the doorsim network using your smartphone or computer.

Open a web browser and navigate to http://192.168.4.1 to access the web interface.

Use the web interface to configure settings, add credentials, and view card data.

## Web Interface
The web interface provides the following features:

View Card Data: Displays a list of previously read cards.

Manage Credentials: Add or remove valid credentials.

Configure Settings: Adjust system settings such as display timeout, WiFi settings, and custom messages.

## File Structure
```
project-folder/
├── data/                  # HTML, CSS, and JavaScript files for the web interface
|   ├── credentials.json
|   ├── favicon.ico
│   ├── index.html
|   ├── settings.json
│   ├── style.css
│   └── script.js
├── src/                   # Source code
│   └── main.cpp
├── platformio.ini         # PlatformIO configuration file
└── README.md              # this file
```

## Acknowledgments
This project is a refactored version of the [DoorSim](https://github.com/evildaemond/doorsim) project by evildaemond. Special thanks to the original developer for their work.

Thanks to the creators of the libraries used in this project.

Inspired by various open-source access control systems.
