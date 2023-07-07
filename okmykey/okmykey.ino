#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>

// #define DEBUG

//////////////////////////////////
///// LEDs configuration
//////////////////////////////////
#define PIN 6
#define NUMPIXELS 4
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Use an international keyboard layout otherwise use the english one
#define INTERNATIONAL_LAYOUT

#ifdef INTERNATIONAL_LAYOUT
#include <KeyboardMultiLanguage.h> // https://github.com/MichaelDworkin/KeyboardMultiLanguage.git
#include "KeyboardMapping.h"
#else
#include <Keyboard.h>
#endif

#define VERSION "1.1"

//////////////////////////////////
///// Buttons Configurations v1.0
//////////////////////////////////
#define S1 10
#define S2 16
#define S3 14
#define S4 15
#define S5 A0
#define S6 A1
#define S7 A2
#define S8 A3

const int numButtons = 8;
const int numPages = 4;
byte switches[numButtons] = {S1, S2, S3, S4, S5, S6, S7, S8};
int selectedPage = 0;
const int numVirtualButtons = numButtons * numPages;

//////////////////////////////////
///// Buttons Debounce
//////////////////////////////////
unsigned long debounceDelay = 125; // the debounce time in ms; increase if the output flickers
long minimumPressTime = 60;        // in ms. increase to reduce phantom presses
long longPressTime = 500;          // in ms
unsigned long lastDebounceTimes[numButtons] = {};
unsigned long buttonPressedTimes[numButtons] = {};
bool lastButtonsStates[numButtons] = {};

const int code_size = 2;
const int code_address = 0;
bool unlocked = false;
byte code_index = 0;
int entered_code = 0;
const int code_length = 4;
bool wrong_code = false;
unsigned long last_flashing_time = 0;
const unsigned long flashing_period = 200;
unsigned long last_flashing_wrong_code_time = 0;
const unsigned long flashing_wrong_code_period = 2000;
bool flashing_state = false;

//////////////////////////////////
///// EEPROM Configuration
///// S1 0
///// +
///// S2 128
//////////////////////////////////

int address = 0;
const unsigned int reservedButtonMemory = EEPROM.length() / numVirtualButtons - code_size;
int buttonNum = 0;

//////////////////////////////////
///// Data decode settings
//////////////////////////////////
#define MAX_MACRO_ORDERS 3
#define MAX_COMMAND_LENGTH 64
#define PRESS_MODE '1'
#define PRINT_MODE '2'
#define SEPARATOR "|"

//////////////////////////////////
///// Send a key every few seconds to keep the OS active
//////////////////////////////////
bool keepSystemActive = false;
const unsigned int sendKeyPeriod = 5000; // ms
unsigned long lastActiveKeySent = 0;

struct button_settings
{
  char raw[MAX_COMMAND_LENGTH * MAX_MACRO_ORDERS + 10];
  unsigned int buttonNumber;
  char mode;
  char commands[MAX_MACRO_ORDERS][MAX_COMMAND_LENGTH];
  unsigned int commandsNum;
};

void sendInfo()
{
  char infoStr[50];
  // Reserve 5 bytes for the header settings (button number, mode and separators)
  sprintf(infoStr, "Info: %d %s %d %d", numVirtualButtons, VERSION, MAX_COMMAND_LENGTH - 5 - MAX_MACRO_ORDERS, MAX_MACRO_ORDERS);
  Serial.println(infoStr);
}

//////////////////////////////////
///// Buttons Commands
//////////////////////////////////
void sendKeyPress(int key)
{
  // Retrieve the button settings
  char data[reservedButtonMemory];
  int virtualKey = key + selectedPage * numButtons;
  getButtonConfiguration(virtualKey, data);

  // Decode the button settings
  struct button_settings settings;
  if (!decodeData(data, &settings))
    return;

  // Execute all the saved commands
  for (unsigned int i = 0; i < settings.commandsNum; i++)
  {
#ifdef DEBUG
    Serial.print("Sending command: ");
    char debugString[reservedButtonMemory];
    sprintf(debugString, "Button: %d, Mode: %c, Command: %x", virtualKey, settings.mode, settings.commands[i][0]);
    Serial.println(debugString);
#endif
    if (settings.mode == PRESS_MODE)
      Keyboard.press(settings.commands[i][0]);
    else if (settings.mode == PRINT_MODE)
      Keyboard.print(settings.commands[i]);
  }

  Keyboard.releaseAll();
}

void changePage(int buttonNumber)
{
  if (buttonNumber == 7)
  {
    keepSystemActive = !keepSystemActive;
    return;
  }

  if (buttonNumber < 0 or buttonNumber > numPages)
    return;

  selectedPage = buttonNumber;
}

void getButtonConfiguration(int buttonNumber, char *data)
{
  // Start address pointer
  unsigned int addressPointer = buttonNumber * reservedButtonMemory + code_size;

  // Read the string
  unsigned char readChar;
  const unsigned char dataLength = EEPROM.read(addressPointer++);
  for (unsigned int len = 0; len < dataLength; len++)
  {
    readChar = EEPROM.read(addressPointer + len);
    data[len] = readChar;
  }
  data[dataLength] = '\0';
}

void setButtonConfiguration(int buttonNumber, const char *data)
{
  // Start address pointer
  unsigned int addressPointer = buttonNumber * reservedButtonMemory + code_size;

  unsigned int dataLength = strlen(data);
  EEPROM.write(addressPointer++, dataLength);
  for (int addrPos = 0; addrPos < dataLength; addrPos++)
  {
    EEPROM.write(addressPointer + addrPos, data[addrPos]);
  }
}

bool validateCode(const char *code)
{
  if (strlen(code) != 4)
  {
    return false; // Code length is not 4
  }

  for (int i = 0; i < 4; i++)
  {
    if (code[i] < '1' || code[i] > '8')
    {
      return false; // Digit is outside the valid range
    }
  }

  return true; // Code is valid
}

void setCode(const char *code)
{
  if (validateCode(code))
  {
    writeCodeToEEPROM(atoi(code));
  }
}

void clearEEPROM()
{
  Serial.println("Clearing EEPROM...");
  for (int i = 0; i < EEPROM.length(); i++)
  {
    EEPROM.write(i, 0x00);
  }
  Serial.println("EEPROM cleared.");
}

void initSettings()
{
  Serial.println("Init settings...");
  char buttonKey[7];
  for (int i = 0; i < numVirtualButtons; i++)
  {
    sprintf(buttonKey, "%d|%c|%d", i, PRINT_MODE, i + 1);
    setButtonConfiguration(i, buttonKey);
  }

  writeCodeToEEPROM(1735);
  Serial.println("Settings initialized.");
}

void setup()
{
  // Setup LEDs
  pixels.begin();
  pixels.setBrightness(100);

  for (int i = 0; i < numVirtualButtons; i++)
  {
    pinMode(switches[i], INPUT);
    digitalWrite(switches[i], HIGH);
  }
#ifdef INTERNATIONAL_LAYOUT
  Keyboard.language(Language_Layout);
#else
  Keyboard.begin();
#endif
  int code = readCodeFromEEPROM();
  Serial.print("Setup code: ");
  Serial.println(code);

  unlocked = code == 0;
}

bool decodeData(char *data, struct button_settings *decodedSettings)
{

  strcpy(decodedSettings->raw, data);

  // Work with local data instead of modifying the original string
  char receivedData[reservedButtonMemory];
  strcpy(receivedData, data);

  const char *pch;
  // First, decode the button number
  pch = strtok(receivedData, SEPARATOR);
  if (pch == NULL)
    return false;
  char *errCheck;
  decodedSettings->buttonNumber = (int)strtol(pch, &errCheck, 10); // String to int
#ifdef DEBUG
  Serial.print("Button number: ");
  Serial.println(decodedSettings->buttonNumber);
#endif
  if (errCheck == pch || *errCheck != '\0' || decodedSettings->buttonNumber < 0 || decodedSettings->buttonNumber >= numVirtualButtons)
    return false;

  // Second, decode the push mode
  pch = strtok(NULL, SEPARATOR);
  if (pch == NULL)
    return false;

  decodedSettings->mode = pch[0];
#ifdef DEBUG
  Serial.print("Push mode: ");
  Serial.println(decodedSettings->mode);
#endif
  if (decodedSettings->mode != PRESS_MODE && decodedSettings->mode != PRINT_MODE)
    return false;

  unsigned int commandsNum = 0;
  // Iterate every value within the separator
  pch = strtok(NULL, SEPARATOR);
  while (pch != NULL && commandsNum < MAX_MACRO_ORDERS)
  {
    unsigned int commandLength = strlen(pch);
#ifdef DEBUG
    Serial.print("Command: ");
    Serial.println(pch);
#endif
    if (commandLength > MAX_COMMAND_LENGTH)
      return false;
    strcpy(decodedSettings->commands[commandsNum++], pch);
    pch = strtok(NULL, SEPARATOR);
  }

  decodedSettings->commandsNum = commandsNum;

  return commandsNum > 0;
}

void writeCodeToEEPROM(int code)
{
  byte lowByte = lowByte(code);   // Extract the low byte
  byte highByte = highByte(code); // Extract the high byte

  EEPROM.write(code_address, lowByte);      // Write the low byte to EEPROM
  EEPROM.write(code_address + 1, highByte); // Write the high byte to EEPROM
}

int readCodeFromEEPROM()
{
  byte lowByte = EEPROM.read(code_address);      // Read the low byte from EEPROM
  byte highByte = EEPROM.read(code_address + 1); // Read the high byte from EEPROM

  int code = highByte << 8 | lowByte; // Combine the bytes into a 16-bit integer

  return code;
}

void sendButtonConfiguration(char *data)
{
  char receivedData[reservedButtonMemory];
  strcpy(receivedData, data);

  const char *pch;
  // First, decode the button number
  pch = strtok(receivedData, " ");
  pch = strtok(NULL, " ");
  if (pch == NULL)
    return;

  char *errCheck;
  int buttonNumber = (int)strtol(pch, &errCheck, 10); // String to int
  if (errCheck == pch || *errCheck != '\0' || buttonNumber < 0 || buttonNumber >= numVirtualButtons)
    return;

  char buttonSettings[reservedButtonMemory];
  getButtonConfiguration(buttonNumber, buttonSettings);

  char dataToSend[reservedButtonMemory];
  sprintf(dataToSend, "button settings: %s", buttonSettings);
  Serial.println(dataToSend);
}

void recvData()
{
  bool newData = false;
  static byte ndx = 0;
  byte MSBbyte = 0;
  const char endMarker = '\n';
  unsigned char rc;
  char receivedChars[reservedButtonMemory];

  while (Serial.available() > 0 && newData == false)
  {
    int byteReceived = Serial.read();

    ////////////////////////////////
    // Decode an extended char code. Credits: https://forum.arduino.cc/index.php?topic=523467.0
    ////////////////////////////////
    // If the previous byte was not extended
    if (MSBbyte == 0)
    {
      // If this byte is the MSB of an extended byte
      if ((byteReceived & B11100000) == B11000000)
      {
        // Save the byte for later
        MSBbyte = byteReceived;
        continue;
      }
      else
      {
        // This is just a regular byte
        rc = byteReceived;
      }
    }
    else
    {
      // Compose the extended byte with the one previously received
      rc = (MSBbyte << 6) | byteReceived;
      MSBbyte = 0;
    }

    // If we have not reached the end of the string
    if (rc != endMarker)
    {
      // Save the char
      receivedChars[ndx] = rc;
      ndx++;
      // Invalid input. Abort.
      if (ndx >= reservedButtonMemory)
      {
        ndx = reservedButtonMemory - 1;
      }
    }
    // We have reached the end of the string
    else
    {
      receivedChars[ndx] = '\0'; // terminate the string
      ndx = 0;
      newData = true;
    }
  }

  // If we have received new data
  if (newData)
  {
    if (strcmp("clear", receivedChars) == 0)
    {
      clearEEPROM();
    }
    else if (strcmp("init", receivedChars) == 0)
    {
      initSettings();
    }
    else if (strstr(receivedChars, "getSettings ") != NULL)
    {
      sendButtonConfiguration(receivedChars);
    }
    else if (strcmp("info", receivedChars) == 0)
    {
      sendInfo();
    }
    else if (strstr(receivedChars, "setCode ") != NULL)
    {
      setCode(receivedChars);
    }
    else
    {
      struct button_settings decodedData;
      if (decodeData(receivedChars, &decodedData))
      {
        setButtonConfiguration(decodedData.buttonNumber, receivedChars);
      }
    }
  }
}

int powerOf10(int exponent)
{
  int result = 1;
  for (int i = 0; i < exponent; i++)
  {
    result *= 10;
  }
  return result;
}

void loop()
{
  // Keyboard loop
  unsigned long currentMillis = millis();
  for (int i = 0; i < numButtons; i++)
  {
    // Debounce the button
    if ((currentMillis - lastDebounceTimes[i]) > debounceDelay)
    {
      bool currentState = digitalRead(switches[i]);
      // Rising edge. Save press time
      if (currentState && lastButtonsStates[i] != currentState)
      {
        buttonPressedTimes[i] = currentMillis;
      }
      // Send the key press if it has been passed more than [longPressTime]ms with the button pressed
      // Long Press
      if (unlocked && currentState && (int)(currentMillis - buttonPressedTimes[i]) > longPressTime)
      {
#ifdef DEBUG
        Serial.print("Long Press button: ");
        Serial.println(i);
#endif
        changePage(i);
        buttonPressedTimes[i] = currentMillis + 10000; // Do not resend the order for 10000ms if the key keeps pressed
      }
      // Falling edge.
      if (!currentState && lastButtonsStates[i] != currentState)
      {
        // Short Press
        if ((int)(currentMillis - buttonPressedTimes[i]) > minimumPressTime)
        {
#ifdef DEBUG
          Serial.print("Short Press button: ");
          Serial.println(i);
#endif
          if (unlocked)
          {
            sendKeyPress(i);
          }
          else
          {
            entered_code += powerOf10(code_length - 1 - code_index) * (i + 1);
#ifdef DEBUG
            Serial.print("entered_code: ");
            Serial.println(entered_code);
#endif
            code_index++;
            if (code_index >= code_length)
            {
              code_index = 0;
              int saved_code = readCodeFromEEPROM();
              if (entered_code == saved_code)
              {
                unlocked = true;
                wrong_code = false;
              }
              else
              {
                wrong_code = true;
                last_flashing_wrong_code_time = currentMillis;
              }
#ifdef DEBUG
              Serial.print("Verifying code: ");
              Serial.println(saved_code);
              Serial.print("Unlocked: ");
              Serial.println(unlocked);
#endif
              entered_code = 0;
            }
          }
          buttonPressedTimes[i] = currentMillis + 10000; // Do not resend the order for 10000ms if the key keeps pressed
        }

        lastDebounceTimes[i] = currentMillis;
      }
      lastButtonsStates[i] = currentState;
    }
  }

  recvData();

  // Keep system active
  if (keepSystemActive && currentMillis - lastActiveKeySent > sendKeyPeriod)
  {
    Keyboard.press(KEY_LEFT_CTRL);
    Keyboard.releaseAll();
    lastActiveKeySent = currentMillis;
  }

  // LEDs loop
  pixels.clear(); // Set all pixel colors to 'off'
  uint32_t color = pixels.Color(50, 150, 20);
  if (unlocked)
  {
    if (keepSystemActive)
    {
      color = pixels.Color(150, 20, 0);
    }
    pixels.setPixelColor(selectedPage, color);
  }
  // LED lights for code indication
  else
  {
    // Change the flashing state
    if ((currentMillis - last_flashing_time) > flashing_period)
    {
      flashing_state = !flashing_state;
      last_flashing_time = currentMillis;
    }

    // Stop the wrong code flashing after some time
    if ((currentMillis - last_flashing_wrong_code_time) > flashing_wrong_code_period)
    {
      wrong_code = false;
    }

    if (flashing_state)
    {
      // Flash all the lights if the code is wrong
      if (wrong_code)
      {
        // Set the state of all the lights
        for (int i = 0; i < code_length; i++)
        {
          pixels.setPixelColor(i, color);
        }
      }
      // Flash the current digit index
      else
      {
        pixels.setPixelColor(code_index, color);
      }
    }
  }
  pixels.show();
}