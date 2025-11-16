#ifndef DOORSIM_H
#define DOORSIM_H

#include <Arduino.h>

// Structs
struct CardData
{
    unsigned int bitCount;
    unsigned long facilityCode;
    unsigned long cardNumber;
    String hexCardData;
    String rawCardData;
    String status;
    String details;
};

struct Credential
{
    unsigned long facilityCode;
    unsigned long cardNumber;
    char name[50];
};


void ISR_INT0();
void ISR_INT1();
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
unsigned long decodeHIDFacilityCode(unsigned int start, unsigned int end);
unsigned long decodeHIDCardNumber(unsigned int start, unsigned int end);
void setCardChunkBits(unsigned int cardChunk1Offset, unsigned int bitHolderOffset, unsigned int cardChunk2Offset);
String prefixPad(const String &in, const char c, const size_t len);
void processHIDCard();
void processCardData();
void clearDatabits();
void cleanupCardData();
bool allBitsAreOnes();
String centerText(const String &text, int width);
void printWelcomeMessage();
void updateDisplay();
void printAllCardData();
void setupWifi();
void webServer();

#endif // DOORSIM_H
