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

#include "doorsim.h"

AsyncWebServer server(80);

const char *settingsFile = "/settings.json";
const char *credentialsFile = "/credentials.json";
const char *wiegandFormatsFile = "/wiegand_formats.json";

// I2C Pins (defaults; may be overridden by settings.json at runtime)
int i2cScl = 18;
int i2cSda = 19;

// Rotary Encoder Pins
int rotaryClk = 25;
int rotaryDt = 26;
int rotarySw = 27;

// Define reader input pins
// card reader DATA0
int data0Pin = 21;
// card reader DATA1
int data1Pin = 22;

// define reader output pins
// LED Output for a GND tie back
int ledPin = 32;
// Speaker Output for a GND tie back
int spkPin = 33;


// Set the LCD I2C address
LiquidCrystal_I2C lcd(0x27, 20, 4); //address may be 0x27, 0x20, or something

// general device settings
bool isCapturing = true;
String MODE = "user";

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
int spkOnInvalid = 1;
int spkOnValid = 1;
int ledValid = 1;

// Custom Display Message
String customBanner = "SHORTRANGE TECH";

// decoded facility code and card code
unsigned long facilityCode = 0;
unsigned long cardNumber = 0;

// raw data string
String rawCardData;
String status;
String details;

// breaking up card value into 2 chunks to create 10 char HEX value



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
int wiegandFormatCount = 0;

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
  Serial.println("Saving settings to Preferences...");

  File file = LittleFS.open(settingsFile, "w");
  if (!file)
  {
    Serial.println("Failed to open settings file for writing.");
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
  doc["spkOnInvalid"] = spkOnInvalid;
  doc["spkOnValid"] = spkOnValid;
  doc["ledValid"] = ledValid;
  doc["customMessage"] = customBanner;
  // pins and timing
  doc["i2c_scl"] = i2cScl;
  doc["i2c_sda"] = i2cSda;
  doc["rotary_clk"] = rotaryClk;
  doc["rotary_dt"] = rotaryDt;
  doc["rotary_sw"] = rotarySw;
  doc["data0"] = data0Pin;
  doc["data1"] = data1Pin;
  doc["led_pin"] = ledPin;
  doc["spk_pin"] = spkPin;
  doc["max_bits"] = maxBits;
  doc["wiegand_wait_time"] = weigandWaitTime;

  if (serializeJson(doc, file) == 0)
  {
    Serial.println("Failed to write settings to file.");
  }
  else
  {
    Serial.println("customMessage: " + customBanner);
    Serial.println("Settings saved successfully.");
  }
  file.close();
  Serial.println("Settings saved!");
}

void loadSettingsFromPreferences()
{
  if (!LittleFS.exists(settingsFile))
  {
    Serial.println("Settings file does not exist. Creating with defaults...");
    saveSettingsToPreferences();
    return;
  }

  File file = LittleFS.open(settingsFile, "r");
  if (!file)
  {
    Serial.println("Failed to open settings file for reading.");
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
    Serial.print("Failed to parse settings file: ");
    Serial.println(error.c_str());
    return;
  }

  // Load settings
  MODE = doc["MODE"] | "user";
  displayTimeout = doc["displayTimeout"] | 30000;
  ap_mode = doc["ap_mode"] | true;
  ap_ssid = doc["ap_ssid"] | "doorsim";
  ap_passphrase = doc["ap_passphrase"] | "";
  ap_channel = doc["ap_channel"] | 1;
  ssid_hidden = doc["ssid_hidden"] | 0;
  spkOnInvalid = doc["spkOnInvalid"] | 1;
  spkOnValid = doc["spkOnValid"] | 1;
  ledValid = doc["ledValid"] | 1;
  customBanner = doc["customMessage"] | "SHORTRANGE TECH";
  // Load pins and timing (clamp maxBits to the compile-time array size)
  i2cScl = doc["i2c_scl"] | i2cScl;
  i2cSda = doc["i2c_sda"] | i2cSda;
  rotaryClk = doc["rotary_clk"] | rotaryClk;
  rotaryDt = doc["rotary_dt"] | rotaryDt;
  rotarySw = doc["rotary_sw"] | rotarySw;
  data0Pin = doc["data0"] | data0Pin;
  data1Pin = doc["data1"] | data1Pin;
  ledPin = doc["led_pin"] | ledPin;
  spkPin = doc["spk_pin"] | spkPin;
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
  Serial.println("Settings loaded successfully:");
}

void saveCredentialsToPreferences()
{
  File file = LittleFS.open(credentialsFile, "w");
  if (!file)
  {
    Serial.println("Failed to open credentials file for writing.");
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
    Serial.println("Failed to write credentials to file.");
  }
  else
  {
    Serial.println("credentials saved successfully.");
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
  Serial.println("Loading Wiegand formats from JSON...");

  if (!LittleFS.exists(wiegandFormatsFile))
  {
    Serial.println("Wiegand formats file does not exist.");
    return;
  }

  File file = LittleFS.open(wiegandFormatsFile, "r");
  if (!file)
  {
    Serial.println("Failed to open wiegand formats file for reading.");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error)
  {
    Serial.print("Failed to parse wiegand formats .json file: ");
    Serial.println(error.c_str());
    return;
  }

  JsonArray formatsArray = doc["wiegandFormats"].as<JsonArray>();
  wiegandFormatCount = 0;

  for (JsonObject format : formatsArray)
  {
    if (wiegandFormatCount >= MAX_WIEGAND_FORMATS)
    {
      Serial.println("Maximum Wiegand formats reached.");
      break;
    }

    wiegandFormats[wiegandFormatCount].bitCount = format["bitCount"] | 0;
    wiegandFormats[wiegandFormatCount].facilityCodeStart = format["facilityCodeStart"] | 0;
    wiegandFormats[wiegandFormatCount].facilityCodeEnd = format["facilityCodeEnd"] | 0;
    wiegandFormats[wiegandFormatCount].cardNumberStart = format["cardNumberStart"] | 0;
    wiegandFormats[wiegandFormatCount].cardNumberEnd = format["cardNumberEnd"] | 0;
    // Pure-binary mode: offsets are no longer used

    Serial.print("Loaded format: bitCount=");
    Serial.println(wiegandFormats[wiegandFormatCount].bitCount);
    wiegandFormatCount++;
  }

  Serial.print("Total Wiegand formats loaded: ");
  Serial.println(wiegandFormatCount);
}

void loadCredentialsFromPreferences()
{
  Serial.println("Loading credentials from Preferences...");

  if (!LittleFS.exists(credentialsFile))
  {
    Serial.println("credentials file does not exist. Creating with defaults...");
    saveCredentialsToPreferences();
    return;
  }

  File file = LittleFS.open(credentialsFile, "r");
  if (!file)
  {
    Serial.println("Failed to open credentials file for reading.");
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
    Serial.print("Failed to parse settings file: ");
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
    Serial.println("No valid credentials found.");
  }
  Serial.println("Credentials loaded from Preferences:");
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
    digitalWrite(ledPin, LOW);
    delay(250);
    digitalWrite(ledPin, HIGH);
    delay(100);
    digitalWrite(ledPin, LOW);
    delay(250);
    digitalWrite(ledPin, HIGH);
    break;

  case 2:
    digitalWrite(ledPin, LOW);
    delay(2000);
    digitalWrite(ledPin, HIGH);
    break;
  }
}

// Functions to handle valid credentials
void speakerOnValid()
{
  switch (spkOnValid)
  {
  case 0:
    break;

  case 1:
    // Nice Beeps LED
    digitalWrite(spkPin, LOW);
    delay(100);
    digitalWrite(spkPin, HIGH);
    delay(50);
    digitalWrite(spkPin, LOW);
    delay(100);
    digitalWrite(spkPin, HIGH);
    break;

  case 2:
    // Long Beeps
    digitalWrite(spkPin, LOW);
    delay(2000);
    digitalWrite(spkPin, HIGH);
    break;
  }
}

// Functions to handle invalid credentials
void lcdInvalidCredentials()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Card Read: ");
  lcd.setCursor(11, 0);
  lcd.print("INVALID");
  lcd.setCursor(0, 2);
  lcd.print(" THIS INCIDENT WILL");
  lcd.setCursor(0, 3);
  lcd.print("    BE REPORTED    ");
}

void speakerOnFailure()
{
  switch (spkOnInvalid)
  {
  case 0:
    break;

  case 1:
    // Sad Beeps
    digitalWrite(spkPin, LOW);
    delay(100);
    digitalWrite(spkPin, HIGH);
    delay(50);
    digitalWrite(spkPin, LOW);
    delay(100);
    digitalWrite(spkPin, HIGH);
    delay(50);
    digitalWrite(spkPin, LOW);
    delay(100);
    digitalWrite(spkPin, HIGH);
    delay(50);
    digitalWrite(spkPin, LOW);
    delay(100);
    digitalWrite(spkPin, HIGH);
    break;
  }
}

void printCardData()
{
  if (MODE == "user")
  {
    const Credential *result = checkCredential(facilityCode, cardNumber);
    if (result != nullptr)
    {
      // Valid credential found
      Serial.println("Valid credential found:");
      Serial.println("FC: " + String(result->facilityCode) + ", CN: " + String(result->cardNumber) + ", Name: " + result->name);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Card Read: ");
      lcd.setCursor(11, 0);
      lcd.print("VALID");
      lcd.setCursor(0, 1);
      lcd.print("FC: " + String(result->facilityCode));
      lcd.setCursor(9, 1);
      lcd.print("CN:" + String(result->cardNumber));
      lcd.setCursor(0, 3);
      lcd.print("Name: " + String(result->name));
      ledOnValid();
      speakerOnValid();

      // Update card data status and details
      status = "Authorized";
      details = result->name;
    }
    else
    {
      // No valid credential found
      Serial.println("Error: No valid credential found.");
      lcdInvalidCredentials();
      speakerOnFailure();

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
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Card Read: ");
      lcd.setCursor(11, 0);
      lcd.print(bitCount);
      lcd.print("bits");
      lcd.setCursor(0, 1);
      lcd.print("FC: ");
      lcd.print(facilityCode);
      lcd.setCursor(9, 1);
      lcd.print(" CN: ");
      lcd.print(cardNumber);
      lcd.setCursor(0, 3);
      lcd.print("Raw: ");
      lcd.print(rawCardData);

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
unsigned long decodeHIDFacilityCode(unsigned int start, unsigned int end)
{
  unsigned long HIDFacilityCode = 0;
  for (unsigned int i = start; i < end; i++)
  {
    HIDFacilityCode = (HIDFacilityCode << 1) | databits[i];
  }
  return HIDFacilityCode;
}

unsigned long decodeHIDCardNumber(unsigned int start, unsigned int end)
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
  // see http://www.pagemac.com/projects/rfid/hid_data_formats for more info
  // also specifically: www.brivo.com/app/static_data/js/calculate.js
  // Example of full card value
  // |>   preamble   <| |>   Actual card value   <|
  // 000000100000000001 11 111000100000100100111000
  // |> write to chunk1 <| |>  write to chunk2   <|

  Serial.print("[*] Bit length: ");
  Serial.println(bitCount);

  // Find the matching Wiegand format
  WiegandFormat *format = nullptr;
  for (int i = 0; i < wiegandFormatCount; i++)
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
  facilityCode = decodeHIDFacilityCode(format->facilityCodeStart, format->facilityCodeEnd);
  cardNumber = decodeHIDCardNumber(format->cardNumberStart, format->cardNumberEnd);

  // Pure-binary mode: facilityCode and cardNumber are derived directly
  // from databits[]. No hex/card chunk generation is performed.
}

void processCardData()
{
  Serial.println("Processing card data...");
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
  Serial.println("Clearing databits...");
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

void displaySetupMassage(const char *message)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(centerText("Setup", 20));
  lcd.setCursor(0, 2);
  lcd.print(centerText(message, 20));
}

void printWelcomeMessage()
{
  if (MODE == "CTF")
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(centerText(customBanner, 20));
    lcd.setCursor(0, 2);
    lcd.print(centerText("Present Card", 20));
  }
  else
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(centerText("Door Sim - Ready", 20));
    lcd.setCursor(0, 2);
    lcd.print(centerText("Present Card", 20));
  }
}

void updateDisplay()
{
  if (displayingCard && (millis() - lastCardTime >= displayTimeout))
  {
    printWelcomeMessage();
    displayingCard = false;
  }
}

void printAllCardData()
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
      doc["apSsid"] = ap_ssid;
      doc["apPassphrase"] = ap_passphrase;
      doc["ssidHidden"] = ssid_hidden;
      doc["apChannel"] = ap_channel;
      doc["customMessage"] = customBanner;
      doc["ledValid"] = ledValid;
      doc["spkOnValid"] = spkOnValid;
      doc["spkOnInvalid"] = spkOnInvalid;
      String response;
      serializeJson(doc, response);
      request->send(200, "application/json", response); });

  AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/saveSettings", [](AsyncWebServerRequest *request, JsonVariant &json)
                                                                         {
      JsonObject jsonObj = json.as<JsonObject>();

      // Parse the JSON and update settings
      MODE = jsonObj["mode"] | "CTF";
      displayTimeout = jsonObj["displayTimeout"] | 30000;
      ap_ssid = jsonObj["apSsid"] | "doorsim";
      ap_passphrase = jsonObj["apPassphrase"] | "";
      ap_channel = jsonObj["apChannel"] | 1;
      ssid_hidden = jsonObj["ssidHidden"] | 0;
      customBanner = jsonObj["customMessage"] | "SHORTRANGE TECH";
      spkOnInvalid = jsonObj["spkOnInvalid"] | 1;
      spkOnValid = jsonObj["spkOnValid"] | 1;
      ledValid = jsonObj["ledValid"] | 1;

      saveSettingsToPreferences();
      //setupWifi();
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
  pinMode(data0Pin, INPUT);
  pinMode(data1Pin, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(spkPin, OUTPUT);


  // turn off led
  digitalWrite(ledPin, HIGH);
  // turn off buzzers
  digitalWrite(spkPin, HIGH);


  Serial.begin(115200);
  delay(100);
  Serial.println("Starting DoorSim...");

  Serial.println("LCD Initialized");
  lcd.init(i2cSda, i2cScl);
  lcd.backlight();
  displaySetupMassage("Initializing...");
  attachInterrupt(data0Pin, ISR_INT0, FALLING);
  attachInterrupt(data1Pin, ISR_INT1, FALLING);

  weigandCounter = weigandWaitTime;
  for (unsigned char i = 0; i < MAX_BITS_CONST; i++)
  {
    lastWrittenDatabits[i] = 0;
  }

  displaySetupMassage("Mounting LittleFS...");

  Serial.println("Checking for LittleFS...");
  if (!LittleFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  loadWiegandFormats();
  loadSettingsFromPreferences();
  loadCredentialsFromPreferences();

  displaySetupMassage("Setup WiFi...");
  Serial.println("Setup Wifi...");
  setupWifi();
  Serial.println("Wifi Setup Complete");

  displaySetupMassage("Starting Web Server...");

  Serial.println("Starting web server...");
  webServer();

  printWelcomeMessage();

  Serial.println("DoorSim Ready!");
}

void loop() {
  updateDisplay();

  // Check if the card reader is still receiving data
  if (!flagDone) {
    if (--weigandCounter == 0) {
      flagDone = 1;  // No more data expected
      Serial.println("Weigand transmission complete.");
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
      if (bitCount >= 26 && bitCount <= 36 || bitCount == 96) {
        // Display card data on LCD and Serial
        printCardData();
        // Print all stored card data to Serial
        printAllCardData();
      }
    }

    // Reset the card reader data for the next read
    cleanupCardData();
    clearDatabits();
  }
}