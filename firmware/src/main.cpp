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

// Display objects - allocated at runtime based on active_display_type
LiquidCrystal_I2C *lcdDisplay = nullptr;
Adafruit_SSD1306 *oledDisplay = nullptr;

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


const char *settingsFile = "/settings.json";
const char *credentialsFile = "/credentials.json";
const char *wiegandFormatsFile = "/wiegand_formats.json";

// I2C Pins (compile-time constants)
#define I2C_SCL 18
#define I2C_SDA 19

// Rotary Encoder Pins (compile-time constants)
#define ROTARY_CLK 25
#define ROTARY_DT 26
#define ROTARY_SW 27

// Define reader input pins (compile-time constants)
// card reader DATA0
#define DATA0_PIN 21
// card reader DATA1
#define DATA1_PIN 22

// define reader output pins (compile-time constants)
// LED Output for a GND tie back
#define LED_PIN 32

#define DISPLAY_LCD 1
#define DISPLAY_OLED_32 2
#define DISPLAY_OLED_64 3

int active_display_type = DISPLAY_LCD; // 1 for LCD, 2 for OLED 128x32, 3 for OLED 128x64

// Set the LCD I2C address
 //address may be 0x27, 0x20, or something

// general device settings
bool isCapturing = true;
String MODE = "access"; // "access" or "raw"

// card reader config and variables

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
bool ap_mode = true;
// AP Settings
// ssid_hidden = broadcast ssid = 0, hidden = 1
// ap_passphrase = NULL for open, min 8 chars, max 63
String ap_ssid = "doorsim";
String ap_passphrase;
int ap_channel = 1;
int ssid_hidden;

// Speaker and LED Settings
int ledValid = 1;

// Custom Display Message
String customMessage = "OPENDOORSIM";

// decoded facility code and card code
unsigned long facilityCode = 0;
unsigned long cardNumber = 0;

// raw data string
String rawCardData;
String status;
String details;


const int MAX_CREDENTIALS = 100;
Credential credentials[MAX_CREDENTIALS];
int validCount = 0;

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
  doc["MODE"] = MODE;
  doc["displayTimeout"] = displayTimeout;
  doc["ap_mode"] = ap_mode;
  doc["ap_ssid"] = ap_ssid;
  doc["ap_passphrase"] = ap_passphrase;
  doc["ap_channel"] = ap_channel;
  doc["ssid_hidden"] = ssid_hidden;
  doc["ledValid"] = ledValid;
  doc["customMessage"] = customMessage;
  // pins and timing (pin constants are compile-time; not stored in settings)
  doc["max_bits"] = maxBits;
  doc["wiegand_wait_time"] = weigandWaitTime;
  doc["active_display_type"] = active_display_type; // 1 for LCD, 2 for OLED 128x32, 3 for OLED 128x64

  if (serializeJson(doc, file) == 0)
  {
    Serial.println("[SYSTEM] ERROR: Failed to write settings to file.");
  }
  else
  {
    Serial.println("customMessage: " + customMessage);
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
    Serial.print("[SYSTEM] ERROR: Failed to parse settings file: ");
    Serial.println(error.c_str());
    return;
  }

  // Load settings
  MODE = doc["MODE"] | "access";
  displayTimeout = doc["displayTimeout"] | 30000;
  ap_mode = doc["ap_mode"] | true;
  ap_ssid = doc["ap_ssid"] | "doorsim";
  ap_passphrase = doc["ap_passphrase"] | "";
  ap_channel = doc["ap_channel"] | 1;
  ssid_hidden = doc["ssid_hidden"] | 0;
  ledValid = doc["ledValid"] | 1;
  customMessage = doc["customMessage"] | "OPENDOORSIM";
  // Load pins and timing (clamp maxBits to the compile-time array size)
  // Pin values are compile-time constants and not loaded from settings
  active_display_type = doc["active_display_type"] | active_display_type; 

  unsigned int loadedMaxBits = doc["max_bits"] | (unsigned int)MAX_BITS_CONST;
  if (loadedMaxBits == 0)
  {
    loadedMaxBits = MAX_BITS_CONST;
  }
  if (loadedMaxBits > MAX_BITS_CONST)
  {
    maxBits = MAX_BITS_CONST;
  }
  else
  {
    maxBits = loadedMaxBits;
  }
  weigandWaitTime = doc["wiegand_wait_time"] | weigandWaitTime;
  Serial.println("[SYSTEM] Settings loaded successfully:");
}

void saveCredentialsToPreferences()
{
  File file = LittleFS.open(credentialsFile, "w");
  if (!file)
  {
    Serial.println("[SYSTEM] ERROR: Failed to open credentials file for writing.");
    return;
  }

  // Write settings to JSON
  JsonDocument doc;
  doc["validCount"] = validCount;

  JsonArray credentialsArray = doc["credentials"].to<JsonArray>();

  for (int i = 0; i < validCount; i++)
  {
    JsonObject credential = credentialsArray.add<JsonObject>();
    credential["facilityCode"] = credentials[i].facilityCode;
    credential["cardNumber"] = credentials[i].cardNumber;
    credential["name"] = credentials[i].name;
    credential["flag"] = credentials[i].flag;
  }

  if (serializeJson(doc, file) == 0)
  {
    Serial.println("[SYSTEM] ERROR: Failed to write credentials to file.");
  }
  else
  {
    Serial.println("[SYSTEM] Credentials saved successfully.");
  }
  file.close();

  for (int i = 0; i < validCount; i++)
  {
    Serial.print("Credential ");
    Serial.print(i);
    Serial.print(": FC=");
    Serial.print(credentials[i].facilityCode);
    Serial.print(", CN=");
    Serial.print(credentials[i].cardNumber);
    Serial.print(", Name=");
    Serial.println(credentials[i].name);
    Serial.print(", Flag=");
    Serial.println(credentials[i].flag);
  }
  Serial.print("Valid Count: ");
  Serial.println(validCount);
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

void loadCredentialsFromPreferences()
{
  Serial.println("[SYSTEM] Loading credentials from Preferences...");

  if (!LittleFS.exists(credentialsFile))
  {
    Serial.println("[SYSTEM] ALERT: Credentials file does not exist. Creating with defaults...");
    saveCredentialsToPreferences();
    return;
  }

  File file = LittleFS.open(credentialsFile, "r");
  if (!file)
  {
    Serial.println("[SYSTEM] ERROR: Failed to open credentials file for reading.");
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
    Serial.print("[SYSTEM] ERROR: Failed to parse settings file: ");
    Serial.println(error.c_str());
    return;
  }

  // Load settings
  validCount = doc["validCount"] | 0;
  if (validCount > 0)
  {
    JsonArray credentialsArray = doc["credentials"].as<JsonArray>();
    for (int i = 0; i < validCount; i++)
    {
      Serial.println("Loading credential " + String(i));
      JsonObject credential = credentialsArray[i].as<JsonObject>();
      credentials[i].facilityCode = credential["facilityCode"] | 0;
      credentials[i].cardNumber = credential["cardNumber"] | 0;
      String name = credential["name"] | "";
      strncpy(credentials[i].name, name.c_str(), sizeof(credentials[i].name) - 1);
      credentials[i].name[sizeof(credentials[i].name) - 1] = '\0';
      String flag = credential["flag"] | "";
      strncpy(credentials[i].flag, flag.c_str(), sizeof(credentials[i].flag) - 1);
      credentials[i].flag[sizeof(credentials[i].flag) - 1] = '\0';
    }
  }
  else
  {
    Serial.println("[SYSTEM] ALERT: No valid credentials found.");
  }
  Serial.println("[SYSTEM] Credentials loaded from Preferences:");
  for (int i = 0; i < validCount; i++)
  {
    Serial.print("Credential ");
    Serial.print(i);
    Serial.print(": FC=");
    Serial.print(credentials[i].facilityCode);
    Serial.print(", CN=");
    Serial.print(credentials[i].cardNumber);
    Serial.print(", Name=");
    Serial.println(credentials[i].name);
    Serial.print(", Flag=");
    Serial.println(credentials[i].flag);
  }
  Serial.print("Valid Count: ");
  Serial.println(validCount);
}

// Check if credential is valid
const Credential *checkCredential(uint16_t fc, uint16_t cn)
{
  for (unsigned int i = 0; i < validCount; i++)
  {
    if (credentials[i].facilityCode == fc && credentials[i].cardNumber == cn)
    {
      // Found a matching credential, return a pointer to it
      return &credentials[i];
    }
  }
  // No matching credential found, return nullptr
  return nullptr;
}

void ledOnValid()
{
  switch (ledValid)
  {
  case 0:
    break;

  case 1:
    // Flashing LED
    digitalWrite(LED_PIN, LOW);
    delay(250);
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(250);
    digitalWrite(LED_PIN, HIGH);
    break;

  case 2:
    digitalWrite(LED_PIN, LOW);
    delay(2000);
    digitalWrite(LED_PIN, HIGH);
    break;
  }
}

void printCardData()
{
  if (MODE == "access")
  {
    const Credential *result = checkCredential(facilityCode, cardNumber);
    if (result != nullptr)
    {
      // Valid credential found - serial console
      Serial.println("Valid credential found:");
      Serial.println("FC: " + String(result->facilityCode) + ", CN: " + String(result->cardNumber) + ", Name: " + result->name);
     
      // LCD Printing
      printDisplayText("   ACCESS GRANTED   ","",centerText("Welcome, " + String(result->name), 20).c_str(),result->flag);

      ledOnValid();

      // Update card data status and details
      status = "Authorized";
      details = result->name;
    }
    else
    {
      // No valid credential found - serial console
      Serial.println("Error: No valid credential found.");

      // LCD Printing 
      printDisplayText("   ACCESS DENIED    ", ""," THIS INCIDENT WILL ","    BE REPORTED!    ");

      // Update card data status and details
      status = "Unauthorized";
      details = "FC: " + String(facilityCode) + ", CN: " + String(cardNumber);
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

      // LCD Printing
      printDisplayRawCard();

      // Update card data status and details
      status = "Read";
      details = "Raw: " + rawCardData;
    }
  }

  // Store card data
  if (cardDataIndex < MAX_CARDS)
  {
    cardDataArray[cardDataIndex].bitCount = bitCount;
    cardDataArray[cardDataIndex].facilityCode = facilityCode;
    cardDataArray[cardDataIndex].cardNumber = cardNumber;
    cardDataArray[cardDataIndex].rawCardData = rawCardData;
    cardDataArray[cardDataIndex].status = status;
    cardDataArray[cardDataIndex].details = details;
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
  // clear the databits array
  rawCardData = "";
  for (unsigned int i = 0; i < bitCount; i++)
  {
    rawCardData += String(databits[i]);
  }

  Serial.print("[*] Raw: ");
  Serial.println(rawCardData);
  Serial.print("[*] bitCount: ");
  Serial.println(bitCount);

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
  rawCardData = "";
  bitCount = 0;
  facilityCode = 0;
  cardNumber = 0;
  status = "";
  details = "";
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
  
  if (active_display_type == DISPLAY_LCD)
  {
    lcdDisplay = new LiquidCrystal_I2C(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);
    lcdDisplay->init(I2C_SDA, I2C_SCL);
    lcdDisplay->backlight();
    lcdDisplay->clear();
    lcdDisplay->setCursor(0, 0);
    lcdDisplay->print("Initializing...");
    Serial.println("[SYSTEM] LCD 20x4 initialized");
  }
  else if (active_display_type == DISPLAY_OLED_32)
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
  else if (active_display_type == DISPLAY_OLED_64)
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
  if (active_display_type == DISPLAY_LCD && lcdDisplay != nullptr)
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
  else if (active_display_type == DISPLAY_OLED_32 && oledDisplay != nullptr)
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
  else if (active_display_type == DISPLAY_OLED_64 && oledDisplay != nullptr)
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
  if (active_display_type == DISPLAY_LCD && lcdDisplay != nullptr)
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
    lcdDisplay->setCursor(0, 2);
    lcdDisplay->print("Raw: ");
    lcdDisplay->setCursor(0, 3);
    lcdDisplay->print(rawCardData);
  }
  else if ((active_display_type == DISPLAY_OLED_32 || active_display_type == DISPLAY_OLED_64) && oledDisplay != nullptr)
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
    oledDisplay->println("Raw:");
    oledDisplay->println(rawCardData);
    oledDisplay->display();
  }
}


void printStandbyMessage()
{
  if (MODE == "access")
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
    Serial.println(cardDataArray[i].rawCardData);
  }
}

void setupWifi()
{
  WiFi.softAP(ap_ssid, ap_passphrase, ap_channel, ssid_hidden);
}

void webServer()
{

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { 
              //request->send(200, "text/html", FPSTR(index_html)); 
              request->send(LittleFS, "/index.html", String()); });

  server.on("/getCards", HTTP_GET, [](AsyncWebServerRequest *request)
            {      
      JsonDocument doc;
      JsonArray cards = doc.to<JsonArray>();
      for (int i = 0; i < cardDataIndex; i++) {          
          JsonObject card = cards.add<JsonObject>();
          card["bitCount"] = cardDataArray[i].bitCount;
          card["facilityCode"] = cardDataArray[i].facilityCode;
          card["cardNumber"] = cardDataArray[i].cardNumber;
          card["rawCardData"] = cardDataArray[i].rawCardData;
          card["status"] = cardDataArray[i].status;
          card["details"] = cardDataArray[i].details;
      }
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response); });

  server.on("/getUsers", HTTP_GET, [](AsyncWebServerRequest *request)
            {
      JsonDocument doc;
      JsonArray users = doc.to<JsonArray>();
      for (int i = 0; i < validCount; i++) {          
          JsonObject user = users.add<JsonObject>();
          user["facilityCode"] = credentials[i].facilityCode;
          user["cardNumber"] = credentials[i].cardNumber;
          user["name"] = credentials[i].name;
          user["flag"] = credentials[i].flag;
      }
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response); });

  server.on("/getSettings", HTTP_GET, [](AsyncWebServerRequest *request)
            {      
      JsonDocument doc;
      doc["mode"] = MODE;
      doc["displayTimeout"] = displayTimeout;
      doc["ap_ssid"] = ap_ssid;
      doc["ap_pwd"] = ap_passphrase;
      doc["ssid_hidden"] = ssid_hidden;
      doc["ap_channel"] = ap_channel;
      doc["customMessage"] = customMessage;
      doc["ledValid"] = ledValid;
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response); });

  AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/saveSettings", [](AsyncWebServerRequest *request, JsonVariant &json)
                                                                         {
      JsonObject jsonObj = json.as<JsonObject>();

      // Parse the JSON and update settings
      MODE = jsonObj["mode"] | "access";
      displayTimeout = jsonObj["displayTimeout"] | 30000;
      ap_ssid = jsonObj["ap_ssid"] | "doorsim";
      ap_passphrase = jsonObj["ap_pwd"] | "";
      ap_channel = jsonObj["ap_channel"] | 1;
      ssid_hidden = jsonObj["ssid_hidden"] | 0;
      customMessage = jsonObj["customMessage"] | "OPENDOORSIM";
      ledValid = jsonObj["ledValid"] | 1;

      saveSettingsToPreferences();
      //setupWifi(); TODO: implement a reboot button so that they can choose to apply the new wifi settings
      request->send(200, "application/json", "{\"status\":\"success\"}"); });
  server.addHandler(handler);

  server.on("/addCard", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (validCount < MAX_CREDENTIALS) {
      if (request->hasParam("facilityCode") && request->hasParam("cardNumber") && request->hasParam("name")) {
        String facilityCodeStr = request->getParam("facilityCode")->value();
        String cardNumberStr = request->getParam("cardNumber")->value();
        String name = request->getParam("name")->value();
        String flag = request->hasParam("flag") ? request->getParam("flag")->value() : "";

        credentials[validCount].facilityCode = facilityCodeStr.toInt();
        credentials[validCount].cardNumber = cardNumberStr.toInt();
        strncpy(credentials[validCount].name, name.c_str(), sizeof(credentials[validCount].name) - 1);
        credentials[validCount].name[sizeof(credentials[validCount].name) - 1] = '\0';
        strncpy(credentials[validCount].flag, flag.c_str(), sizeof(credentials[validCount].flag) - 1);
        credentials[validCount].flag[sizeof(credentials[validCount].flag) - 1] = '\0';
        validCount++;
        saveCredentialsToPreferences();
        request->send(200, "text/plain", "Card added successfully");
      } else {
        request->send(400, "text/plain", "Missing parameters");
      }
    } else {
      request->send(500, "text/plain", "Max number of credentials reached");
    } });

  server.on("/deleteCard", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (request->hasParam("index")) {
      int index = request->getParam("index")->value().toInt();
      if (index >= 0 && index < validCount) {
        for (int i = index; i < validCount - 1; i++) {
          credentials[i] = credentials[i + 1];
        }
        validCount--;
        saveCredentialsToPreferences();
        request->send(200, "text/plain", "Card deleted successfully");
      } else {
        request->send(400, "text/plain", "Invalid index");
      }
    } else {
      request->send(400, "text/plain", "Missing index parameter");
    } });

  server.on("/exportData", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    JsonDocument doc;
    JsonArray users = doc.to<JsonArray>();
    JsonObject user = users.add<JsonObject>();
    for (int i = 0; i < validCount; i++) {
        JsonObject user = users.add<JsonObject>();
        user["facilityCode"] = credentials[i].facilityCode;
        user["cardNumber"] = credentials[i].cardNumber;
        user["name"] = credentials[i].name;
        user["flag"] = credentials[i].flag;
    }
    JsonArray cards = doc["cards"].to<JsonArray>();    
    for (int i = 0; i < cardDataIndex; i++) {
        JsonObject card = cards.add<JsonObject>();
        card["bitCount"] = cardDataArray[i].bitCount;
        card["facilityCode"] = cardDataArray[i].facilityCode;
        card["cardNumber"] = cardDataArray[i].cardNumber;
        card["rawCardData"] = cardDataArray[i].rawCardData;
    }
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response); });

  // Route to load style.css file, and script.js file
  server.serveStatic("/", LittleFS, "/");

  server.begin();
}

void setup()
{
  pinMode(DATA0_PIN, INPUT);
  pinMode(DATA1_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

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
  loadCredentialsFromPreferences();

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