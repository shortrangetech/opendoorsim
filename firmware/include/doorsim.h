#ifndef DOORSIM_H
#define DOORSIM_H

#include <Arduino.h>

// Structs
struct CardData
{
    unsigned int bitCount;
    unsigned long facilityCode;
    unsigned long cardNumber;
    String rawCardData;
    String hexData;
    int padCount;
    String status;
    String details;
};

struct Credential
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
void saveCredentialsToPreferences();
void loadCredentialsFromPreferences();
const Credential *checkCredential(uint16_t fc, uint16_t cn);
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
