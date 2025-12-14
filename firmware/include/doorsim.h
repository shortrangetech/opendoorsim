#ifndef DOORSIM_H
#define DOORSIM_H

#include <Arduino.h>

// Structs
// Fixed-size buffer sizes to avoid heap fragmentation
#define RAW_DATA_MAX 128
#define HEX_DATA_MAX 64
#define STATUS_MAX 64
#define DETAILS_MAX 128

struct CardData
{
    unsigned int bitCount;
    unsigned long facilityCode;
    unsigned long cardNumber;
    char rawCardData[RAW_DATA_MAX];
    char hexData[HEX_DATA_MAX];
    int padCount;
    char status[STATUS_MAX];
    char details[DETAILS_MAX];
};

struct User
{
    unsigned long facilityCode;
    unsigned long cardNumber;
    char name[50];
    char flag[50];
};

struct WiegandFormat
{
    unsigned int bitCount;
    unsigned int facilityCodeStart;
    unsigned int facilityCodeEnd;
    unsigned int cardNumberStart;
    unsigned int cardNumberEnd;
};


void ISR_INT0();
void ISR_INT1();
void loadWiegandFormats();
void saveSettingsToPreferences();
void loadSettingsFromPreferences();
void saveUsersToPreferences();
void loadUsersFromPreferences();
const User *checkUser(unsigned long fc, unsigned long cn);
void ledOnValid();
void speakerOnValid();
void lcdInvalidCredentials();
void speakerOnFailure();
void printCardData();
unsigned long decodeFacilityCode(unsigned int start, unsigned int end);
unsigned long decodeCardNumber(unsigned int start, unsigned int end);
// Pure-binary mode: no card chunking helper
String prefixPad(const String &in, const char c, const size_t len);
String convertBinaryToHex(const String &binaryString, int &outPadCount);
void processHIDCard();
void processCardData();
void clearDatabits();
void cleanupCardData();
bool allBitsAreOnes();
String centerText(const String &text, int width);
void printStandbyMessage();
void updateDisplay();
void printCardDataSerial();
void setupWifi();
void webServer();
void initializeDisplay();
void printDisplayText(const char *msg1, const char *msg2, const char *msg3, const char *msg4);
void printDisplayRawCard();

#endif // DOORSIM_H
