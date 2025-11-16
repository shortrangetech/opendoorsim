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

#define I2C_SDA 21
#define I2C_SCL 22
// Set the LCD I2C address
LiquidCrystal_I2C lcd(0x20, 20, 4);

// general device settings
bool isCapturing = true;
String MODE = "CTF";

// card reader config and variables

// max number of bits
#define MAX_BITS 100
// time to wait for another weigand pulse
#define WEIGAND_WAIT_TIME 3000

// stores all of the data bits
volatile unsigned char databits[MAX_BITS];
volatile unsigned int bitCount = 0;

// stores the last written card's data bits
unsigned char lastWrittenDatabits[MAX_BITS];
unsigned int lastWrittenBitCount = 0;

// goes low when data is currently being captured
volatile unsigned char flagDone;

// countdown until we assume there are no more bits
volatile unsigned int weigandCounter;

// Display screen timer
unsigned long displayTimeout = 30000; // 30 seconds
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
String customMessage;
String welcomeMessage = "default";

// decoded facility code and card code
unsigned long facilityCode = 0;
unsigned long cardNumber = 0;

// hex data string
String hexCardData;

// raw data string
String rawCardData;
String status;
String details;

// breaking up card value into 2 chunks to create 10 char HEX value
volatile unsigned long bitHolder1 = 0;
volatile unsigned long bitHolder2 = 0;
unsigned long cardChunk1 = 0;
unsigned long cardChunk2 = 0;

// Define reader input pins
// card reader DATA0
#define DATA0 19
// card reader DATA1
#define DATA1 18

// define reader output pins
//  LED Output for a GND tie back
#define LED 32
// Speaker Output for a GND tie back
#define SPK 33

// define relay modules
#define RELAY1 25
#define RELAY2 26

const int MAX_CREDENTIALS = 100;
Credential credentials[MAX_CREDENTIALS];
int validCount = 0;

// maximum number of stored cards
const int MAX_CARDS = 100;
CardData cardDataArray[MAX_CARDS];
int cardDataIndex = 0;

// Interrupts for card reader
void ISR_INT0()
{
  bitCount++;
  flagDone = 0;

  if (bitCount < 23)
  {
    bitHolder1 = bitHolder1 << 1;
  }
  else
  {
    bitHolder2 = bitHolder2 << 1;
  }
  // Reset the wait timer
  weigandCounter = WEIGAND_WAIT_TIME;
}

// interrupt that happens when INT1 goes low (1 bit)
void ISR_INT1()
{
  if (bitCount < MAX_BITS)
  {
    databits[bitCount] = 1;
    bitCount++;
  }
  flagDone = 0;

  if (bitCount < 23)
  {
    bitHolder1 = bitHolder1 << 1;
    bitHolder1 |= 1;
  }
  else
  {
    bitHolder2 = bitHolder2 << 1;
    bitHolder2 |= 1;
  }
  // Reset the wait timer
  weigandCounter = WEIGAND_WAIT_TIME;
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
  doc["customMessage"] = customMessage;
  doc["welcomeMessage"] = welcomeMessage;

  if (serializeJson(doc, file) == 0)
  {
    Serial.println("Failed to write settings to file.");
  }
  else
  {
    Serial.println("customMessage: " + customMessage);
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
  MODE = doc["MODE"] | "CTF";
  displayTimeout = doc["displayTimeout"] | 30000;
  ap_mode = doc["ap_mode"] | true;
  ap_ssid = doc["ap_ssid"] | "doorsim";
  ap_passphrase = doc["ap_passphrase"] | "";
  ap_channel = doc["ap_channel"] | 1;
  ssid_hidden = doc["ssid_hidden"] | 0;
  spkOnInvalid = doc["spkOnInvalid"] | 1;
  spkOnValid = doc["spkOnValid"] | 1;
  ledValid = doc["ledValid"] | 1;
  welcomeMessage = doc["welcomeMessage"] | "default";
  if (welcomeMessage != "default")
  {
    customMessage = doc["customMessage"] | "";
  }
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
  JsonArray credentialsArray = doc.createNestedArray("credentials");
  for (int i = 0; i < validCount; i++)
  {
    JsonObject credential = credentialsArray.createNestedObject();
    credential["facilityCode"] = credentials[i].facilityCode;
    credential["cardNumber"] = credentials[i].cardNumber;
    credential["name"] = credentials[i].name;
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
  }
  Serial.print("Valid Count: ");
  Serial.println(validCount);
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
    digitalWrite(LED, LOW);
    delay(250);
    digitalWrite(LED, HIGH);
    delay(100);
    digitalWrite(LED, LOW);
    delay(250);
    digitalWrite(LED, HIGH);
    break;

  case 2:
    digitalWrite(LED, LOW);
    delay(2000);
    digitalWrite(LED, HIGH);
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
    digitalWrite(SPK, LOW);
    delay(100);
    digitalWrite(SPK, HIGH);
    delay(50);
    digitalWrite(SPK, LOW);
    delay(100);
    digitalWrite(SPK, HIGH);
    break;

  case 2:
    // Long Beeps
    digitalWrite(SPK, LOW);
    delay(2000);
    digitalWrite(SPK, HIGH);
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
    digitalWrite(SPK, LOW);
    delay(100);
    digitalWrite(SPK, HIGH);
    delay(50);
    digitalWrite(SPK, LOW);
    delay(100);
    digitalWrite(SPK, HIGH);
    delay(50);
    digitalWrite(SPK, LOW);
    delay(100);
    digitalWrite(SPK, HIGH);
    delay(50);
    digitalWrite(SPK, LOW);
    delay(100);
    digitalWrite(SPK, HIGH);
    break;
  }
}

void printCardData()
{
  if (MODE == "CTF")
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
      Serial.print("[*] Hex: ");
      Serial.println(hexCardData);
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
      lcd.print("Hex: ");
      hexCardData.toUpperCase();
      lcd.print(hexCardData);

      // Update card data status and details
      status = "Read";
      details = "Hex: " + hexCardData;
    }
  }

  // Store card data
  if (cardDataIndex < MAX_CARDS)
  {
    cardDataArray[cardDataIndex].bitCount = bitCount;
    cardDataArray[cardDataIndex].facilityCode = facilityCode;
    cardDataArray[cardDataIndex].cardNumber = cardNumber;
    cardDataArray[cardDataIndex].hexCardData = hexCardData;
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

// Card value processing functions
// Function to append the card value (bitHolder1 and bitHolder2) to the
// necessary array then translate that to the two chunks for the card value that
// will be output
void setCardChunkBits(unsigned int cardChunk1Offset, unsigned int bitHolderOffset, unsigned int cardChunk2Offset)
{
  for (int i = 19; i >= 0; i--)
  {
    if (i == 13 || i == cardChunk1Offset)
    {
      bitWrite(cardChunk1, i, 1);
    }
    else if (i > cardChunk1Offset)
    {
      bitWrite(cardChunk1, i, 0);
    }
    else
    {
      bitWrite(cardChunk1, i, bitRead(bitHolder1, i + bitHolderOffset));
    }
    if (i < bitHolderOffset)
    {
      bitWrite(cardChunk2, i + cardChunk2Offset, bitRead(bitHolder1, i));
    }
    if (i < cardChunk2Offset)
    {
      bitWrite(cardChunk2, i, bitRead(bitHolder2, i));
    }
  }
}

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

  unsigned int cardChunk1Offset, bitHolderOffset, cardChunk2Offset;

  Serial.print("[*] Bit length: ");
  Serial.println(bitCount);
  switch (bitCount)
  {
  case 26:
    facilityCode = decodeHIDFacilityCode(1, 9);
    cardNumber = decodeHIDCardNumber(9, 25);
    cardChunk1Offset = 2;
    bitHolderOffset = 20;
    cardChunk2Offset = 4;
    break;

  case 27:
    facilityCode = decodeHIDFacilityCode(1, 13);
    cardNumber = decodeHIDCardNumber(13, 27);
    cardChunk1Offset = 3;
    bitHolderOffset = 19;
    cardChunk2Offset = 5;
    break;

  case 29:
    facilityCode = decodeHIDFacilityCode(1, 13);
    cardNumber = decodeHIDCardNumber(13, 29);
    cardChunk1Offset = 5;
    bitHolderOffset = 17;
    cardChunk2Offset = 7;
    break;

  case 30:
    facilityCode = decodeHIDFacilityCode(1, 13);
    cardNumber = decodeHIDCardNumber(13, 29);
    cardChunk1Offset = 6;
    bitHolderOffset = 16;
    cardChunk2Offset = 8;
    break;

  case 31:
    facilityCode = decodeHIDFacilityCode(1, 5);
    cardNumber = decodeHIDCardNumber(5, 28);
    cardChunk1Offset = 7;
    bitHolderOffset = 15;
    cardChunk2Offset = 9;
    break;

  // modified to wiegand 32 bit format instead of HID
  case 32:
    facilityCode = decodeHIDFacilityCode(5, 16);
    cardNumber = decodeHIDCardNumber(17, 32);
    cardChunk1Offset = 8;
    bitHolderOffset = 14;
    cardChunk2Offset = 10;
    break;

  case 33:
    facilityCode = decodeHIDFacilityCode(1, 8);
    cardNumber = decodeHIDCardNumber(8, 32);
    cardChunk1Offset = 9;
    bitHolderOffset = 13;
    cardChunk2Offset = 11;
    break;

  case 34:
    facilityCode = decodeHIDFacilityCode(1, 17);
    cardNumber = decodeHIDCardNumber(17, 33);
    cardChunk1Offset = 10;
    bitHolderOffset = 12;
    cardChunk2Offset = 12;
    break;

  case 35:
    facilityCode = decodeHIDFacilityCode(2, 14);
    cardNumber = decodeHIDCardNumber(14, 34);
    cardChunk1Offset = 11;
    bitHolderOffset = 11;
    cardChunk2Offset = 13;
    break;

  case 36:
    facilityCode = decodeHIDFacilityCode(21, 33);
    cardNumber = decodeHIDCardNumber(1, 17);
    cardChunk1Offset = 12;
    bitHolderOffset = 10;
    cardChunk2Offset = 14;
    break;

  default:
    Serial.println("[-] Unsupported bitCount for HID card");
    return;
  }

  setCardChunkBits(cardChunk1Offset, bitHolderOffset, cardChunk2Offset);
  hexCardData = String(cardChunk1, HEX) + prefixPad(String(cardChunk2, HEX), '0', 6);
  // hexCardData = String(cardChunk1, HEX) + String(cardChunk2, HEX);
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
  for (unsigned char i = 0; i < MAX_BITS; i++)
  {
    databits[i] = 0;
  }
}

// reset variables and prepare for the next card read
void cleanupCardData()
{
  rawCardData = "";
  hexCardData = "";
  bitCount = 0;
  facilityCode = 0;
  cardNumber = 0;
  bitHolder1 = 0;
  bitHolder2 = 0;
  cardChunk1 = 0;
  cardChunk2 = 0;
  status = "";
  details = "";
}

bool allBitsAreOnes()
{
  for (int i = 0; i < MAX_BITS; i++)
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
    if (customMessage != NULL)
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(centerText(String(customMessage), 20));
      lcd.setCursor(0, 2);
      lcd.print(centerText("Present Card", 20));
    }
    else
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(centerText("CTF Mode", 20));
      lcd.setCursor(0, 2);
      lcd.print(centerText("Present Card", 20));
    }
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
    Serial.print(", Hex: ");
    Serial.print(cardDataArray[i].hexCardData);
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
          card["hexCardData"] = cardDataArray[i].hexCardData;
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
      doc["welcomeMessage"] = customMessage.length() > 0 ? "custom" : "default";
      doc["customMessage"] = customMessage;
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
      welcomeMessage = jsonObj["welcomeMessage"] | "default";
      customMessage = jsonObj["customMessage"] | "";
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

        credentials[validCount].facilityCode = facilityCodeStr.toInt();
        credentials[validCount].cardNumber = cardNumberStr.toInt();
        strncpy(credentials[validCount].name, name.c_str(), sizeof(credentials[validCount].name) - 1);
        credentials[validCount].name[sizeof(credentials[validCount].name) - 1] = '\0';
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
    }
    JsonArray cards = doc["cards"].to<JsonArray>();    
    for (int i = 0; i < cardDataIndex; i++) {
        JsonObject card = cards.add<JsonObject>();
        card["bitCount"] = cardDataArray[i].bitCount;
        card["facilityCode"] = cardDataArray[i].facilityCode;
        card["cardNumber"] = cardDataArray[i].cardNumber;
        card["hexCardData"] = cardDataArray[i].hexCardData;
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
  pinMode(DATA0, INPUT);
  pinMode(DATA1, INPUT);
  pinMode(LED, OUTPUT);
  pinMode(SPK, OUTPUT);
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);

  // turn off led
  digitalWrite(LED, HIGH);
  // turn off buzzers
  digitalWrite(SPK, HIGH);
  // turn on relay, lock the door!
  digitalWrite(RELAY1, HIGH);

  Serial.begin(115200);
  delay(100);
  Serial.println("Starting DoorSim...");

  Serial.println("LCD Initialized");
  lcd.init(I2C_SDA, I2C_SCL);
  lcd.backlight();
  displaySetupMassage("Initializing...");

  attachInterrupt(DATA0, ISR_INT0, FALLING);
  attachInterrupt(DATA1, ISR_INT1, FALLING);

  weigandCounter = WEIGAND_WAIT_TIME;
  for (unsigned char i = 0; i < MAX_BITS; i++)
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