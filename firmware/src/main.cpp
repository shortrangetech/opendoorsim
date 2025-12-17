#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include "nvs_flash.h"
#include "AsyncJson.h"
#include "ArduinoJson.h"
#include <LittleFS.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "doorsim.h"

AsyncWebServer server(80);

const char *settingsFile = "/settings.json";
const char *usersFile = "/users.json";
const char *wiegandFormatsFile = "/wiegand_formats.json";

// I2C Pins 
#define I2C_SCL 18
#define I2C_SDA 19

// Rotary Encoder Pins 
#define ROTARY_CLK 25
#define ROTARY_DT 26
#define ROTARY_SW 27

// Reader input pins 
#define DATA0_PIN 21
#define DATA1_PIN 22
  // optional, tamper detection relay
#define RELAY1_PIN 4

// Reader output pins 
#define LED_PIN 16

// Display type constants
#define DISPLAY_LCD 1
#define DISPLAY_OLED_32 2
#define DISPLAY_OLED_64 3

// Display configuration constants
#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 20
#define LCD_ROWS 4

#define OLED_RESET -1
#define OLED_32_ADDRESS 0x3C
#define OLED_64_ADDRESS 0x3D
#define OLED_WIDTH 128
#define OLED_32_HEIGHT 32
#define OLED_64_HEIGHT 64

// Display objects - allocated at runtime based on active_display_type
LiquidCrystal_I2C *lcdDisplay = nullptr;
Adafruit_SSD1306 *oledDisplay = nullptr;

// Active display variable
// 1 for LCD, 2 for OLED 128x32, 3 for OLED 128x64
int activeDisplayType = DISPLAY_LCD; 

// general device settings
bool isCapturing = true;
String deviceMode = "ctf"; // "ctf" or "raw"

// Tamper Relay Settings
bool enableTamperDetect = false;
bool tamperState = false;
unsigned long lastTamperCheck = 0;
const int TAMPER_CHECK_INTERVAL = 500; // milliseconds

// Reboot management
bool rebootRequested = false;
unsigned long rebootTimer = 0;

// Reader config and variables

// compile-time array size for bits (kept as a safe upper bound)
const int MAX_BITS_CONST = 100;
// runtime-configurable max bits (loaded from settings.json)
volatile unsigned int maxBits = MAX_BITS_CONST;
// time to wait for another weigand pulse (runtime-configurable)
volatile unsigned int weigandWaitTime = 30000;

// stores all of the data bits (sized to compile-time const)
volatile unsigned char databits[MAX_BITS_CONST];
volatile unsigned int bitCount = 0;

// stores the last written card's data bits
unsigned char lastWrittenDatabits[MAX_BITS_CONST];
unsigned int lastWrittenBitCount = 0;

// goes low when data is currently being captured
volatile unsigned char flagDone;

// countdown until we assume there are no more bits
volatile unsigned int weigandCounter;

// Display screen timer
unsigned long displayTimeout = 7000; // 7 seconds
unsigned long lastCardTime = 0;
bool displayingCard = false;

// Wifi Settings
bool apMode = true;
// AP Settings
// ssid_hidden = broadcast ssid = 0, hidden = 1
// ap_passphrase = NULL for open, min 8 chars, max 63
String apSsid = "doorsim";
String apPwd;
int apChannel = 1;
int ssidHidden;

// LED Flash Green on Valid Setting 0 (None), 1 (Rapid Flash), 2 (Long Flash) 
int ledValid = 1;

// Custom Display Message
String customMessage = "OPENDOORSIM";
// settings file version (read from settings.json)
String firmwareVersion = "0.7";

// decoded facility code and card code
unsigned long facilityCode = 0;
unsigned long cardNumber = 0;

// raw data string (fixed buffers)
char rawCardData[RAW_DATA_MAX];
char status[STATUS_MAX];
char details[DETAILS_MAX];
char lastHexData[HEX_DATA_MAX];
int lastPadCount = 0;


const int MAX_USERS = 100;
User users[MAX_USERS];
int userCount = 0;

// maximum number of stored cards
const int MAX_CARDS = 100;
CardData cardDataArray[MAX_CARDS];
int cardDataIndex = 0;

// Wiegand formats
const int MAX_WIEGAND_FORMATS = 50;
WiegandFormat wiegandFormats[MAX_WIEGAND_FORMATS];
int wiegandFormatCounter = 0;

// Interrupts for card reader
void ISR_INT0()
{
  // DATA0 pulse represents a 0 bit
  if (bitCount < maxBits)
  {
    databits[bitCount] = 0;
    bitCount++;
  }
  flagDone = 0;
  // Reset the wait timer
  weigandCounter = weigandWaitTime;
}

// interrupt that happens when INT1 goes low (1 bit)
void ISR_INT1()
{
  // DATA1 pulse represents a 1 bit
  if (bitCount < maxBits)
  {
    databits[bitCount] = 1;
    bitCount++;
  }
  flagDone = 0;
  // Reset the wait timer
  weigandCounter = weigandWaitTime;
}

void saveSettingsToPreferences()
{
  Serial.println("[SYSTEM] Saving settings to Preferences...");

  File file = LittleFS.open(settingsFile, "w");
  if (!file)
  {
    Serial.println("[SYSTEM] ERROR: Failed to open settings file for writing.");
    return;
  }

  // Write settings to JSON
  JsonDocument doc;
  doc["device_mode"] = deviceMode;
  doc["display_timeout"] = displayTimeout;
  doc["ap_mode"] = apMode;
  doc["ap_ssid"] = apSsid;
  doc["ap_pwd"] = apPwd;
  doc["ap_channel"] = apChannel;
  doc["ssid_hidden"] = ssidHidden;
  doc["led_valid"] = ledValid;
  doc["custom_message"] = customMessage;
  doc["max_bits"] = maxBits;
  doc["wiegand_wait_time"] = weigandWaitTime;
  doc["active_display_type"] = activeDisplayType; // 1 for LCD, 2 for OLED 128x32, 3 for OLED 128x64
  doc["enable_tamper_detect"] = enableTamperDetect;

  if (serializeJson(doc, file) == 0)
  {
    Serial.println("[SYSTEM] ERROR: Failed to write settings to file.");
  }
  else
  {
    Serial.println("custom_message: " + customMessage);
    Serial.println("[SYSTEM] Settings saved successfully.");
  }
  file.close();
  Serial.println("[SYSTEM] Settings saved!");
}

void loadSettingsFromPreferences()
{
  if (!LittleFS.exists(settingsFile))
  {
    Serial.println("[SYSTEM] ALERT: Settings file does not exist. Creating with defaults...");
    saveSettingsToPreferences();
    return;
  }

  File file = LittleFS.open(settingsFile, "r");
  if (!file)
  {
    Serial.println("[SYSTEM] ERROR: Failed to open settings file for reading.");
    return;
  }

  // --- DEBUG: Print File Content ---
  // This reads the file to the end
  Serial.println("File Content:");
  while (file.available())
  {
    Serial.write(file.read());
  }
  Serial.println();

  // --- FIX STARTS HERE ---
  
  // 1. DO NOT close the file yet.
  // 2. Rewind the file position to the beginning so JSON can read it.
  file.seek(0);

  // 3. Parse JSON from the open file
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);

  // 4. NOW it is safe to close the file
  file.close(); 

  // --- FIX ENDS HERE ---

  if (error)
  {
    Serial.print("[SYSTEM] ERROR: Failed to parse settings file: ");
    Serial.println(error.c_str());
    return;
  }

  // Load settings
  deviceMode = doc["device_mode"] | "ctf";
  displayTimeout = doc["display_timeout"] | 30000;
  apMode = doc["ap_mode"] | true;
  apSsid = doc["ap_ssid"] | "doorsim";
  apPwd = doc["ap_pwd"] | "";
  apChannel = doc["ap_channel"] | 1;
  ssidHidden = doc["ssid_hidden"] | 0;
  ledValid = doc["led_valid"] | 1;
  customMessage = doc["custom_message"] | "OPENDOORSIM";
  activeDisplayType = doc["active_display_type"] | activeDisplayType;
  enableTamperDetect = doc["enable_tamper_detect"] | false;

  unsigned int loadedMaxBits = doc["max_bits"] | (unsigned int)MAX_BITS_CONST;
  if (loadedMaxBits == 0) loadedMaxBits = MAX_BITS_CONST;
  if (loadedMaxBits > MAX_BITS_CONST) maxBits = MAX_BITS_CONST;
  else maxBits = loadedMaxBits;

  weigandWaitTime = doc["wiegand_wait_time"] | weigandWaitTime;
  Serial.println("[SYSTEM] Settings loaded successfully.");
}

void saveUsersToPreferences()
{
  File file = LittleFS.open(usersFile, "w");
  if (!file)
  {
    Serial.println("[SYSTEM] ERROR: Failed to open users file for writing.");
    return;
  }

  // Write users to JSON
  JsonDocument doc;
  doc["userCount"] = userCount;

  JsonArray usersArray = doc["users"].to<JsonArray>();

  for (int i = 0; i < userCount; i++)
  {
    JsonObject user = usersArray.add<JsonObject>();
    user["facilityCode"] = users[i].facilityCode;
    user["cardNumber"] = users[i].cardNumber;
    user["name"] = users[i].name;
    user["flag"] = users[i].flag;
  }

  if (serializeJson(doc, file) == 0)
  {
    Serial.println("[SYSTEM] ERROR: Failed to write users to file.");
  }
  else
  {
    Serial.println("[SYSTEM] Users saved successfully.");
  }
  file.close();

  for (int i = 0; i < userCount; i++)
  {
    Serial.print("User ");
    Serial.print(i);
    Serial.print(": FC=");
    Serial.print(users[i].facilityCode);
    Serial.print(", CN=");
    Serial.print(users[i].cardNumber);
    Serial.print(", Name=");
    Serial.println(users[i].name);
    Serial.print(", Flag=");
    Serial.println(users[i].flag);
  }
  Serial.print("User Count: ");
  Serial.println(userCount);
}

void loadWiegandFormats()
{
  Serial.println("[SYSTEM] Loading Wiegand formats from JSON...");

  if (!LittleFS.exists(wiegandFormatsFile))
  {
    Serial.println("[SYSTEM] ALERT: Wiegand formats file does not exist.");
    return;
  }

  File file = LittleFS.open(wiegandFormatsFile, "r");
  if (!file)
  {
    Serial.println("[SYSTEM] ERROR: Failed to open wiegand formats file for reading.");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error)
  {
    Serial.print("[SYSTEM] ERROR: Failed to parse wiegand formats .json file: ");
    Serial.println(error.c_str());
    return;
  }

  JsonArray formatsArray = doc["wiegandFormats"].as<JsonArray>();
  wiegandFormatCounter = 0;

  for (JsonObject format : formatsArray)
  {
    if (wiegandFormatCounter >= MAX_WIEGAND_FORMATS)
    {
      Serial.println("[SYSTEM] ALERT: Maximum Wiegand formats reached.");
      break;
    }

    wiegandFormats[wiegandFormatCounter].bitCount = format["bitCount"] | 0;
    wiegandFormats[wiegandFormatCounter].facilityCodeStart = format["facilityCodeStart"] | 0;
    wiegandFormats[wiegandFormatCounter].facilityCodeEnd = format["facilityCodeEnd"] | 0;
    wiegandFormats[wiegandFormatCounter].cardNumberStart = format["cardNumberStart"] | 0;
    wiegandFormats[wiegandFormatCounter].cardNumberEnd = format["cardNumberEnd"] | 0;

    Serial.print("Loaded format: bitCount=");
    Serial.println(wiegandFormats[wiegandFormatCounter].bitCount);
    wiegandFormatCounter++;
  }

  Serial.print("[SYSTEM] Total Wiegand formats loaded: ");
  Serial.println(wiegandFormatCounter);
}

void loadUsersFromPreferences()
{
  Serial.println("[SYSTEM] Loading users from Preferences...");

  if (!LittleFS.exists(usersFile))
  {
    Serial.println("[SYSTEM] ALERT: Users file does not exist. Creating with defaults...");
    saveUsersToPreferences();
    return;
  }

  File file = LittleFS.open(usersFile, "r");
  if (!file)
  {
    Serial.println("[SYSTEM] ERROR: Failed to open users file for reading.");
    return;
  }

  // Parse JSON from file
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  Serial.println("File Content:");
  file.seek(0);
  while (file.available())
  {
    Serial.write(file.read());
  }
  file.close();
  Serial.println();

  if (error)
  {
    Serial.print("[SYSTEM] ERROR: Failed to parse users file: ");
    Serial.println(error.c_str());
    return;
  }

  // Load users
  userCount = doc["userCount"] | 0;
  if (userCount > 0)
  {
    JsonArray usersArray = doc["users"].as<JsonArray>();
    for (int i = 0; i < userCount; i++)
    {
      Serial.println("Loading user " + String(i));
      JsonObject user = usersArray[i].as<JsonObject>();
      users[i].facilityCode = user["facilityCode"] | 0;
      users[i].cardNumber = user["cardNumber"] | 0;
      String name = user["name"] | "";
      strncpy(users[i].name, name.c_str(), sizeof(users[i].name) - 1);
      users[i].name[sizeof(users[i].name) - 1] = '\0';
      String flag = user["flag"] | "";
      strncpy(users[i].flag, flag.c_str(), sizeof(users[i].flag) - 1);
      users[i].flag[sizeof(users[i].flag) - 1] = '\0';
    }
  }
  else
  {
    Serial.println("[SYSTEM] ALERT: No valid users found.");
  }
  Serial.println("[SYSTEM] Users loaded from Preferences:");
  for (int i = 0; i < userCount; i++)
  {
    Serial.print("User ");
    Serial.print(i);
    Serial.print(": FC=");
    Serial.print(users[i].facilityCode);
    Serial.print(", CN=");
    Serial.print(users[i].cardNumber);
    Serial.print(", Name=");
    Serial.println(users[i].name);
    Serial.print(", Flag=");
    Serial.println(users[i].flag);
  }
  Serial.print("User Count: ");
  Serial.println(userCount);
}

// Check if user is valid
const User *checkUser(unsigned long fc, unsigned long cn)
{
  for (unsigned int i = 0; i < (unsigned int)userCount; i++)
  {
    if (users[i].facilityCode == fc && users[i].cardNumber == cn)
    {
      return &users[i];
    }
  }
  return nullptr;
}

void ledOnValid()
{
  switch (ledValid)
  {
  case 0:
    // No Flash
    break;

  case 1:
    // Rapid Flash
    digitalWrite(LED_PIN, LOW);
    delay(250);
    digitalWrite(LED_PIN, HIGH);
    delay(250);
    digitalWrite(LED_PIN, LOW);
    delay(250);
    digitalWrite(LED_PIN, HIGH);
    delay(250);
    digitalWrite(LED_PIN, LOW);
    delay(250);
    digitalWrite(LED_PIN, HIGH);
    delay(250);
    digitalWrite(LED_PIN, LOW);
    delay(250);
    digitalWrite(LED_PIN, HIGH);
    break;

  case 2:
  // Long Flash
    digitalWrite(LED_PIN, LOW);
    delay(2000);
    digitalWrite(LED_PIN, HIGH);
    break;
  }
}

void printCardData()
{
  if (deviceMode == "ctf")
  {
    const User *result = checkUser(facilityCode, cardNumber);
    if (result != nullptr)
    {
      // Valid user found - serial console
      Serial.println("Valid user found:");
      Serial.println("FC: " + String(result->facilityCode) + ", CN: " + String(result->cardNumber) + ", Name: " + result->name);

      // LCD Printing
      printDisplayText("   ACCESS GRANTED   ","",centerText("Welcome, " + String(result->name), 20).c_str(),result->flag);

      ledOnValid();

      // Update card data status and details (use fixed buffers)
      strncpy(status, "Authorized", STATUS_MAX - 1);
      status[STATUS_MAX - 1] = '\0';
      strncpy(details, result->name, DETAILS_MAX - 1);
      details[DETAILS_MAX - 1] = '\0';
    }
    else
    {
      // No valid user found - serial console
      Serial.println("Error: No valid user found.");

      // LCD Printing 
      printDisplayText("   ACCESS DENIED    ", ""," THIS INCIDENT WILL ","    BE REPORTED!    ");

      // Update card data status and details (use fixed buffers)
      strncpy(status, "Unauthorized", STATUS_MAX - 1);
      status[STATUS_MAX - 1] = '\0';
      snprintf(details, DETAILS_MAX, "FC: %lu, CN: %lu", facilityCode, cardNumber);
    }
  }
  else
  {
    // ranges for "valid" bitCount are a bit larger for debugging
    if (bitCount > 20 && bitCount < 120)
    {
      // ignore data caused by noise
      Serial.print("[*] Bit length: ");
      Serial.println(bitCount);
      Serial.print("[*] Facility code: ");
      Serial.println(facilityCode);
      Serial.print("[*] Card number: ");
      Serial.println(cardNumber);
      Serial.print("[*] Raw: ");
      Serial.println(rawCardData);

      // Use the hex and pad calculated in processCardData(); delegate to display helper
      printDisplayRawCard();

      // Update card data status and details (use fixed buffers)
      strncpy(status, "RawRead", STATUS_MAX - 1);
      status[STATUS_MAX - 1] = '\0';
      snprintf(details, DETAILS_MAX, "Hex: %s", lastHexData);
    }
  }

  // Store card data
  if (cardDataIndex < MAX_CARDS)
  {
    cardDataArray[cardDataIndex].bitCount = bitCount;
    cardDataArray[cardDataIndex].facilityCode = facilityCode;
    cardDataArray[cardDataIndex].cardNumber = cardNumber;
    strncpy(cardDataArray[cardDataIndex].rawCardData, rawCardData, RAW_DATA_MAX - 1);
    cardDataArray[cardDataIndex].rawCardData[RAW_DATA_MAX - 1] = '\0';
    // Store previously-calculated hex and padding from processCardData()
    strncpy(cardDataArray[cardDataIndex].hexData, lastHexData, HEX_DATA_MAX - 1);
    cardDataArray[cardDataIndex].hexData[HEX_DATA_MAX - 1] = '\0';
    cardDataArray[cardDataIndex].padCount = lastPadCount;

    strncpy(cardDataArray[cardDataIndex].status, status, STATUS_MAX - 1);
    cardDataArray[cardDataIndex].status[STATUS_MAX - 1] = '\0';
    strncpy(cardDataArray[cardDataIndex].details, details, DETAILS_MAX - 1);
    cardDataArray[cardDataIndex].details[DETAILS_MAX - 1] = '\0';
    cardDataIndex++;
  }

  // Start the display timer
  lastCardTime = millis();
  displayingCard = true;
}

// Process hid cards
unsigned long decodeFacilityCode(unsigned int start, unsigned int end)
{
  unsigned long HIDFacilityCode = 0;
  for (unsigned int i = start; i < end; i++)
  {
    HIDFacilityCode = (HIDFacilityCode << 1) | databits[i];
  }
  return HIDFacilityCode;
}

unsigned long decodeCardNumber(unsigned int start, unsigned int end)
{
  unsigned long HIDCardNumber = 0;
  for (unsigned int i = start; i < end; i++)
  {
    HIDCardNumber = (HIDCardNumber << 1) | databits[i];
  }
  return HIDCardNumber;
}

// Card chunking logic removed for Pure Binary mode

String prefixPad(const String &in, const char c, const size_t len)
{
  String out = in;
  while (out.length() < len)
  {
    out = c + out;
  }
  return out;
}

String convertBinaryToHex(const String &binaryString, int &outPadCount)
{
  // Calculate padding needed to reach nearest multiple of 4
  int remainder = binaryString.length() % 4;
  outPadCount = (remainder == 0) ? 0 : (4 - remainder);
  
  // Create padded binary string
  String paddedBinary = "";
  for (int i = 0; i < outPadCount; i++) {
    paddedBinary += "0";
  }
  paddedBinary += binaryString;
  
  // Convert to hexadecimal
  String hexString = "";
  for (int i = 0; i < paddedBinary.length(); i += 4) {
    // Extract 4-bit chunk
    int nibble = 0;
    for (int j = 0; j < 4; j++) {
      nibble = (nibble << 1) | (paddedBinary[i + j] - '0');
    }
    // Convert to hex character (0-9, A-F) - uppercase
    if (nibble < 10) {
      hexString += (char)('0' + nibble);
    } else {
      hexString += (char)('A' + nibble - 10);
    }
  }
  
  return hexString;
}

// C-style converter: accepts a null-terminated binary string of '0'/'1' characters
// and writes an uppercase hex string into outHex (ensures null-termination).
void convertBinaryToHexC(const char *binaryString, int &outPadCount, char *outHex, size_t outHexSize)
{
  size_t len = strlen(binaryString);
  int remainder = len % 4;
  outPadCount = (remainder == 0) ? 0 : (4 - remainder);

  // padded length
  size_t paddedLen = len + outPadCount;
  // Ensure there is enough room in outHex: each 4 bits becomes 1 hex char
  size_t neededHex = (paddedLen / 4) + 1; // +1 for null
  if (outHexSize < neededHex) {
    // not enough space; write empty string
    if (outHexSize > 0) outHex[0] = '\0';
    return;
  }

  // process nibbles from padded binary (pad with leading zeros)
  size_t outPos = 0;
  for (size_t i = 0; i < paddedLen; i += 4)
  {
    int nibble = 0;
    for (int j = 0; j < 4; j++)
    {
      size_t idx = i + j;
      char bitChar;
      if (idx < (size_t)outPadCount) {
        bitChar = '0';
      } else {
        size_t srcIdx = idx - outPadCount;
        bitChar = (srcIdx < len) ? binaryString[srcIdx] : '0';
      }
      nibble = (nibble << 1) | (bitChar - '0');
    }
    if (nibble < 10) outHex[outPos++] = '0' + nibble;
    else outHex[outPos++] = 'A' + (nibble - 10);
  }
  outHex[outPos] = '\0';
}

void processHIDCard()
{
  // bits to be decoded differently depending on card format length
  // see http://www.pagemac.com/projects/rfid/hid_data_formats 

  Serial.print("[*] Bit length: ");
  Serial.println(bitCount);

  // Find the matching Wiegand format
  WiegandFormat *format = nullptr;
  for (int i = 0; i < wiegandFormatCounter; i++)
  {
    if (wiegandFormats[i].bitCount == bitCount)
    {
      format = &wiegandFormats[i];
      break;
    }
  }

  if (format == nullptr)
  {
    Serial.println("[-] Unsupported bitCount for HID card");
    return;
  }

  // Extract facility code and card number using the format
  facilityCode = decodeFacilityCode(format->facilityCodeStart, format->facilityCodeEnd);
  cardNumber = decodeCardNumber(format->cardNumberStart, format->cardNumberEnd);

  // Pure-binary mode: facilityCode and cardNumber are derived directly
  // from databits[]. No hex/card chunk generation is performed.
}

void processCardData()
{
  Serial.println("[SYSTEM] Processing card data...");
  // Build C-style raw binary string into fixed buffer
  if (bitCount >= RAW_DATA_MAX) bitCount = RAW_DATA_MAX - 1;
  for (unsigned int i = 0; i < bitCount; i++)
  {
    rawCardData[i] = databits[i] ? '1' : '0';
  }
  rawCardData[bitCount] = '\0';

  Serial.print("[*] Raw: ");
  Serial.println(rawCardData);
  Serial.print("[*] bitCount: ");
  Serial.println(bitCount);

  // Convert to HEX once here and store pad count for display/storage using safe C routine
  convertBinaryToHexC(rawCardData, lastPadCount, lastHexData, HEX_DATA_MAX);
  Serial.print("[*] Hex: ");
  Serial.println(lastHexData);
  Serial.print("[*] Pad: ");
  Serial.println(lastPadCount);

  if (bitCount >= 26 && bitCount <= 96)
  {
    processHIDCard();
  }
}

void clearDatabits()
{
  Serial.println("[SYSTEM] Clearing databits...");
  // clear the databits array
  for (unsigned char i = 0; i < MAX_BITS_CONST; i++)
  {
    databits[i] = 0;
  }
}

// reset variables and prepare for the next card read
void cleanupCardData()
{
  rawCardData[0] = '\0';
  bitCount = 0;
  facilityCode = 0;
  cardNumber = 0;
  status[0] = '\0';
  details[0] = '\0';
  lastHexData[0] = '\0';
  lastPadCount = 0;
}

bool allBitsAreOnes()
{
  for (int i = 0; i < MAX_BITS_CONST; i++)
  {
    if (databits[i] != 0xFF)
    {               // Check if each byte is not equal to 0xFF
      return false; // If any byte is not 0xFF, not all bits are ones
    }
  }
  return true; // All bytes were 0xFF, so all bits are ones
}

String centerText(const String &text, int width)
{
  int len = text.length();
  if (len >= width)
  {
    return text;
  }
  int padding = (width - len) / 2;
  String spaces = "";
  for (int i = 0; i < padding; i++)
  {
    spaces += " ";
  }
  return spaces + text;
}

void initializeDisplay()
{
  Serial.println("[SYSTEM] Initializing Display...");

  Wire.begin(I2C_SDA, I2C_SCL);
  
  if (activeDisplayType == DISPLAY_LCD)
  {
    lcdDisplay = new LiquidCrystal_I2C(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);
    lcdDisplay->init(I2C_SDA, I2C_SCL);
    lcdDisplay->backlight();
    lcdDisplay->clear();
    lcdDisplay->setCursor(0, 0);
    lcdDisplay->print("Initializing...");
    Serial.println("[SYSTEM] LCD 20x4 initialized");
  }
  else if (activeDisplayType == DISPLAY_OLED_32)
  {
    oledDisplay = new Adafruit_SSD1306(OLED_WIDTH, OLED_32_HEIGHT, &Wire, OLED_RESET);
    if (!oledDisplay->begin(SSD1306_SWITCHCAPVCC, OLED_32_ADDRESS))
    {
      Serial.println("[SYSTEM] ERROR: OLED 128x32 initialization failed!");
      delete oledDisplay;
      oledDisplay = nullptr;
      return;
    }
    oledDisplay->clearDisplay();
    oledDisplay->setTextSize(1);
    oledDisplay->setTextColor(SSD1306_WHITE);
    oledDisplay->setCursor(0, 0);
    oledDisplay->println("Initializing...");
    oledDisplay->display();
    Serial.println("[SYSTEM] OLED 128x32 initialized");
  }
  else if (activeDisplayType == DISPLAY_OLED_64)
  {
    oledDisplay = new Adafruit_SSD1306(OLED_WIDTH, OLED_64_HEIGHT, &Wire, OLED_RESET);
    if (!oledDisplay->begin(SSD1306_SWITCHCAPVCC, OLED_64_ADDRESS))
    {
      Serial.println("[SYSTEM] ERROR: OLED 128x64 initialization failed!");
      delete oledDisplay;
      oledDisplay = nullptr;
      return;
    }
    oledDisplay->clearDisplay();
    oledDisplay->setTextSize(1);
    oledDisplay->setTextColor(SSD1306_WHITE);
    oledDisplay->setCursor(0, 0);
    oledDisplay->println("Initializing...");
    oledDisplay->display();
    Serial.println("[SYSTEM] OLED 128x64 initialized");
  }
}

// Prints four lines of text to the activeDisplayType
void printDisplayText(const char *msg1, const char *msg2, const char *msg3, const char *msg4)
{
  if (activeDisplayType == DISPLAY_LCD && lcdDisplay != nullptr)
  {
    lcdDisplay->clear();
    lcdDisplay->setCursor(0, 0);
    lcdDisplay->print(msg1);
    lcdDisplay->setCursor(0, 1);
    lcdDisplay->print(msg2);
    lcdDisplay->setCursor(0, 2);
    lcdDisplay->print(msg3);
    lcdDisplay->setCursor(0, 3);
    lcdDisplay->print(msg4);
  }
  else if (activeDisplayType == DISPLAY_OLED_32 && oledDisplay != nullptr)
  {
    oledDisplay->clearDisplay();
    oledDisplay->setTextSize(1);
    oledDisplay->setTextColor(SSD1306_WHITE);
    oledDisplay->setCursor(0, 0);
    oledDisplay->println(msg1);
    oledDisplay->println(msg2);
    oledDisplay->println(msg3);
    oledDisplay->println(msg4);
    oledDisplay->display();
  }
  else if (activeDisplayType == DISPLAY_OLED_64 && oledDisplay != nullptr)
  {
    oledDisplay->clearDisplay();
    oledDisplay->setTextSize(1);
    oledDisplay->setTextColor(SSD1306_WHITE);
    oledDisplay->setCursor(0, 0);
    oledDisplay->println(msg1);
    oledDisplay->println(msg2);
    oledDisplay->println(msg3);
    oledDisplay->println(msg4);
    oledDisplay->display();
  }
}

void printDisplayRawCard()
{
  if (activeDisplayType == DISPLAY_LCD && lcdDisplay != nullptr)
  {
    lcdDisplay->clear();
    lcdDisplay->setCursor(0, 0);
    lcdDisplay->print("CARD READ: ");
    lcdDisplay->setCursor(11, 0);
    lcdDisplay->print(bitCount);
    lcdDisplay->print(" bits");
    lcdDisplay->setCursor(0, 1);
    lcdDisplay->print("FC: ");
    lcdDisplay->setCursor(4, 1);
    lcdDisplay->print(facilityCode);
    lcdDisplay->setCursor(9, 1);
    lcdDisplay->print(" CN: ");
    lcdDisplay->setCursor(14, 1);
    lcdDisplay->print(cardNumber);
    // Show HEX and PAD instead of raw binary to save space
    lcdDisplay->setCursor(0, 2);
    lcdDisplay->print("HEX: ");
    lcdDisplay->setCursor(5, 2);
    lcdDisplay->print(lastHexData);
    lcdDisplay->setCursor(0, 3);
    lcdDisplay->print("PAD: ");
    char padBuf[8];
    if (lastPadCount == 0) strncpy(padBuf, "None", sizeof(padBuf)); else snprintf(padBuf, sizeof(padBuf), "%d", lastPadCount);
    lcdDisplay->setCursor(5, 3);
    lcdDisplay->print(padBuf);
  }
  else if ((activeDisplayType == DISPLAY_OLED_32 || activeDisplayType == DISPLAY_OLED_64) && oledDisplay != nullptr)
  {
    oledDisplay->clearDisplay();
    oledDisplay->setTextSize(1);
    oledDisplay->setTextColor(SSD1306_WHITE);
    oledDisplay->setCursor(0, 0);
    oledDisplay->print("CARD: ");
    oledDisplay->print(bitCount);
    oledDisplay->println("b");
    oledDisplay->print("FC:");
    oledDisplay->print(facilityCode);
    oledDisplay->print(" CN:");
    oledDisplay->println(cardNumber);
    oledDisplay->println("HEX:");
    oledDisplay->println(lastHexData);
    oledDisplay->print("PAD: ");
    if (lastPadCount == 0) oledDisplay->println("None"); else oledDisplay->println(lastPadCount);
    oledDisplay->display();
  }
}


void printStandbyMessage()
{
  if (enableTamperDetect && tamperState)
  {
    printDisplayText("   TAMPER ALERT!   ", "", "   THIS INCIDENT    ", "  WILL BE REPORTED! ");
    return;
  }
  
  if (deviceMode == "ctf")
  {
    printDisplayText(centerText(customMessage, 20).c_str(), "", "    Present Card    ", ""); 
  }
  else
  {
    printDisplayText("      RAW MODE      ", "", "    Present Card    ", "");
  }
}

void updateDisplay()
{
  if (displayingCard && (millis() - lastCardTime >= displayTimeout))
  {
    printStandbyMessage();
    displayingCard = false;
  }
}

void printCardDataSerial()
{
  Serial.println("Previously read card data:");
  for (int i = 0; i < cardDataIndex; i++)
  {
    Serial.print(i + 1);
    Serial.print(": Bit length: ");
    Serial.print(cardDataArray[i].bitCount);
    Serial.print(", Facility code: ");
    Serial.print(cardDataArray[i].facilityCode);
    Serial.print(", Card number: ");
    Serial.print(cardDataArray[i].cardNumber);
    Serial.print(", Raw: ");
    Serial.print(cardDataArray[i].rawCardData);
    Serial.print(", Hex: ");
    Serial.print(cardDataArray[i].hexData);
    Serial.print(", Pad: ");
    Serial.println(cardDataArray[i].padCount);
  }
}

void showSettingsSaved()
{
  Serial.println("[DISPLAY] Showing Settings Saved message");
  
  printDisplayText("   CONFIGURATION    ", "","  Settings saved.   ", "");

  // fake "card read" to trigger display timeout
  lastCardTime = millis();
  displayingCard = true; 
}

void setupWifi()
{
  Serial.println("[SYSTEM] Configuring Access Point...");
  
  const char* ssid = apSsid.c_str();
  const char* pwd = nullptr; // Default to NULL (Open Network)

  // WPA2 Requirement: Password must be at least 8 characters
  if (apPwd.length() >= 8) {
      pwd = apPwd.c_str();
      Serial.println("[SYSTEM] Security: WPA2-PSK (Password set)");
  } else if (apPwd.length() > 0) {
      // fallback to open to avoid crash
      Serial.println("[SYSTEM] WARNING: Password '" + apPwd + "' is too short (min 8 chars).");
      Serial.println("[SYSTEM] FALLBACK: Starting as OPEN network to prevent crash.");
      pwd = nullptr; 
  } else {
      Serial.println("[SYSTEM] Security: OPEN (No password set)");
  }

  // Start AP
  if (WiFi.softAP(ssid, pwd, apChannel, ssidHidden)) {
      Serial.println("[SYSTEM] SoftAP started successfully.");
      Serial.print("[SYSTEM] IP Address: ");
      Serial.println(WiFi.softAPIP());
  } else {
      Serial.println("[SYSTEM] CRITICAL ERROR: Failed to start SoftAP!");
  }
}

void webServer()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    // request->send(200, "text/html", FPSTR(index_html)); 
    request->send(LittleFS, "/index.html", String());
  });

  server.on("/getCards", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    JsonArray cards = doc.to<JsonArray>();
    for (int i = 0; i < cardDataIndex; i++) {
      JsonObject card = cards.add<JsonObject>();
      card["bitCount"] = cardDataArray[i].bitCount;
      card["facilityCode"] = cardDataArray[i].facilityCode;
      card["cardNumber"] = cardDataArray[i].cardNumber;
      card["rawCardData"] = cardDataArray[i].rawCardData;
      card["hexData"] = cardDataArray[i].hexData;
      card["padCount"] = cardDataArray[i].padCount;
      card["status"] = cardDataArray[i].status;
      card["details"] = cardDataArray[i].details;
    }
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  server.on("/getUsers", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    JsonArray usersArray = doc.to<JsonArray>();
    for (int i = 0; i < userCount; i++) {
      JsonObject user = usersArray.add<JsonObject>();
      user["facilityCode"] = users[i].facilityCode;
      user["cardNumber"] = users[i].cardNumber;
      user["name"] = users[i].name;
      user["flag"] = users[i].flag;
    }
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  server.on("/getSettings", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["device_mode"] = deviceMode;
    doc["display_timeout"] = displayTimeout;
    doc["ap_ssid"] = apSsid;
    doc["ap_pwd"] = apPwd;
    doc["ap_mode"] = apMode;
    doc["active_display_type"] = activeDisplayType;
    doc["enable_tamper_detect"] = enableTamperDetect;
    doc["tamper_tripped"] = tamperState;
    doc["ssid_hidden"] = ssidHidden;
    doc["ap_channel"] = apChannel;
    doc["custom_message"] = customMessage;
    doc["version"] = firmwareVersion;
    doc["led_valid"] = ledValid;
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/saveSettings", [](AsyncWebServerRequest *request, JsonVariant &json) {
    JsonObject jsonObj = json.as<JsonObject>();

    // --- Update Settings ---
    deviceMode = jsonObj["device_mode"] | "ctf";
    displayTimeout = jsonObj["display_timeout"] | 30000;
    apSsid = jsonObj["ap_ssid"] | "doorsim";

    String newPwd = jsonObj["ap_pwd"] | "";
    if (newPwd.length() > 0) {
      apPwd = newPwd;
    }

    ssidHidden = jsonObj["ssid_hidden"] | 0;
    customMessage = jsonObj["custom_message"] | "OPENDOORSIM";
    ledValid = jsonObj["led_valid"] | 1;
    activeDisplayType = jsonObj["active_display_type"] | activeDisplayType;
    enableTamperDetect = jsonObj["enable_tamper_detect"] | enableTamperDetect;

    // --- Check Reboot Flag from Client ---
    bool clientSaysReboot = jsonObj["should_reboot"] | false;

    saveSettingsToPreferences();
    showSettingsSaved();

    // --- Send Response ---
    request->send(200, "application/json", "{\"status\":\"success\"}");

    // --- Trigger Reboot Sequence if needed ---
    if (clientSaysReboot) {
      Serial.println("[SYSTEM] Configuration changed. Reboot requested.");
      rebootRequested = true;
      rebootTimer = millis(); // Start the countdown clock
    }
  });
  server.addHandler(handler);

  server.on("/addUser", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (userCount < MAX_USERS) {
      if (request->hasParam("facilityCode") && request->hasParam("cardNumber") && request->hasParam("name")) {
        String facilityCodeStr = request->getParam("facilityCode")->value();
        String cardNumberStr = request->getParam("cardNumber")->value();
        String name = request->getParam("name")->value();
        String flag = request->hasParam("flag") ? request->getParam("flag")->value() : "";

        users[userCount].facilityCode = facilityCodeStr.toInt();
        users[userCount].cardNumber = cardNumberStr.toInt();
        strncpy(users[userCount].name, name.c_str(), sizeof(users[userCount].name) - 1);
        users[userCount].name[sizeof(users[userCount].name) - 1] = '\0';
        strncpy(users[userCount].flag, flag.c_str(), sizeof(users[userCount].flag) - 1);
        users[userCount].flag[sizeof(users[userCount].flag) - 1] = '\0';
        userCount++;
        saveUsersToPreferences();
        request->send(200, "text/plain", "User added successfully");
      } else {
        request->send(400, "text/plain", "Missing parameters");
      }
    } else {
      request->send(500, "text/plain", "Max number of users reached");
    }
  });

  server.on("/deleteUser", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("index")) {
      int index = request->getParam("index")->value().toInt();
      if (index >= 0 && index < userCount) {
        for (int i = index; i < userCount - 1; i++) {
          users[i] = users[i + 1];
        }
        userCount--;
        saveUsersToPreferences();
        request->send(200, "text/plain", "User deleted successfully");
      } else {
        request->send(400, "text/plain", "Invalid index");
      }
    } else {
      request->send(400, "text/plain", "Missing index parameter");
    }
  });

  server.on("/updateUser", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("index") && request->hasParam("facilityCode") && request->hasParam("cardNumber") && request->hasParam("name")) {
      int index = request->getParam("index")->value().toInt();
      if (index >= 0 && index < userCount) {
        String facilityCodeStr = request->getParam("facilityCode")->value();
        String cardNumberStr = request->getParam("cardNumber")->value();
        String name = request->getParam("name")->value();
        String flag = request->hasParam("flag") ? request->getParam("flag")->value() : "";

        users[index].facilityCode = facilityCodeStr.toInt();
        users[index].cardNumber = cardNumberStr.toInt();
        strncpy(users[index].name, name.c_str(), sizeof(users[index].name) - 1);
        users[index].name[sizeof(users[index].name) - 1] = '\0';
        strncpy(users[index].flag, flag.c_str(), sizeof(users[index].flag) - 1);
        users[index].flag[sizeof(users[index].flag) - 1] = '\0';
        saveUsersToPreferences();
        request->send(200, "text/plain", "User updated successfully");
      } else {
        request->send(400, "text/plain", "Invalid index");
      }
    } else {
      request->send(400, "text/plain", "Missing parameters");
    }
  });

  server.on("/exportData", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    JsonArray usersArray = doc["users"].to<JsonArray>();
    for (int i = 0; i < userCount; i++) {
      JsonObject user = usersArray.add<JsonObject>();
      user["facilityCode"] = users[i].facilityCode;
      user["cardNumber"] = users[i].cardNumber;
      user["name"] = users[i].name;
      user["flag"] = users[i].flag;
    }
    JsonArray cards = doc["cards"].to<JsonArray>();
    for (int i = 0; i < cardDataIndex; i++) {
      JsonObject card = cards.add<JsonObject>();
      card["bitCount"] = cardDataArray[i].bitCount;
      card["facilityCode"] = cardDataArray[i].facilityCode;
      card["cardNumber"] = cardDataArray[i].cardNumber;
      card["rawCardData"] = cardDataArray[i].rawCardData;
      card["hexData"] = cardDataArray[i].hexData;
      card["padCount"] = cardDataArray[i].padCount;
    }
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // Route to load style.css file, and script.js file
  server.serveStatic("/", LittleFS, "/");

  server.begin();
}

void checkTamper() {
  // Non-blocking timer: only check every X ms
  if (millis() - lastTamperCheck >= TAMPER_CHECK_INTERVAL) {
    lastTamperCheck = millis();

    // Read pin state
    // HIGH = Open Circuit (Reader Removed/Tampered)
    // LOW = Grounded (Reader Mounted/Safe)
    bool currentReading = (digitalRead(RELAY1_PIN) == HIGH);

    // Only act if state has CHANGED
    if (currentReading != tamperState) {
      tamperState = currentReading;

      if (tamperState) {
        Serial.println("[SYSTEM] ALERT! TAMPER DETECTED!");
        // Immediately update display to show alarm
        printStandbyMessage(); 
      } else {
        Serial.println("[SYSTEM] Tamper Restored (Safe).");
        // Restore the standby screen
        printStandbyMessage();
      }
    }
  }
}

void setup()
{
  pinMode(DATA0_PIN, INPUT);
  pinMode(DATA1_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY1_PIN, INPUT_PULLUP);

  // turn off led
  digitalWrite(LED_PIN, HIGH);

  Serial.begin(115200);
  delay(100);

  Serial.println("[SYSTEM] Loading File System and Settings...");

    Serial.println("[SYSTEM] Checking for LittleFS...");
  if (!LittleFS.begin(true))
  {
    Serial.println("[SYSTEM] ERROR: An Error has occurred while mounting LittleFS!");
    return;
  }
  loadWiegandFormats();
  loadSettingsFromPreferences();
  loadUsersFromPreferences();

  if (enableTamperDetect) {
    Serial.println("[SYSTEM] Tamper Detection is ENABLED");
  } else {
    Serial.println("[SYSTEM] Tamper Detection is DISABLED");
  }
  Serial.println("[SYSTEM] Mode is set to: " + deviceMode);
  // INITIALIZE DISPLAY 
  initializeDisplay();

  printDisplayText("    OPENDOORSIM     ", "         by         ", "  SHORTRANGE.TECH   ", "");
  attachInterrupt(DATA0_PIN, ISR_INT0, FALLING);
  attachInterrupt(DATA1_PIN, ISR_INT1, FALLING);

  weigandCounter = weigandWaitTime;
  for (unsigned char i = 0; i < MAX_BITS_CONST; i++)
  {
    lastWrittenDatabits[i] = 0;
  }



  Serial.println("[SYSTEM] Setting up Wifi...");
  setupWifi();
  Serial.println("[SYSTEM] Wifi Setup Complete");

  Serial.println("[SYSTEM] Starting web server...");
  webServer();

  Serial.println("[SYSTEM] DoorSim Ready!");

  delay(4000);
  printStandbyMessage();
}

void loop() {

  if (rebootRequested) {
      // Wait 1000ms so the HTTP 200 OK response has returned to client
      if (millis() - rebootTimer > 1000) {
          Serial.println("[SYSTEM] Rebooting now...");
          ESP.restart();
      }
      return; 
  }

  if (enableTamperDetect) {
    // Check tamper state
    checkTamper();
  }

  updateDisplay();

  // Check if the card reader is still receiving data
  if (!flagDone) {
    if (--weigandCounter == 0) {
      flagDone = 1;  // No more data expected
      Serial.println("[LOOP] Weigand transmission complete.");
    }
  }

  // Check if the card reader has finished reading data
  if (bitCount > 0 && flagDone) {
    // Indicate that a card is being displayed
    displayingCard = true;

    // Ensure the data is valid (not all bits are 1s)
    if (!allBitsAreOnes()) { 
      // Process the card data     
      processCardData();
      // Print the card data if it meets the criteria
      if (bitCount >= 26 && bitCount <= (MAX_BITS_CONST - 4)) {
        // Display card data on LCD and Serial
        printCardData();
        // Print all stored card data to Serial
        printCardDataSerial();
      }
    }

    // Reset the card reader data for the next read
    cleanupCardData();
    clearDatabits();
  }
}