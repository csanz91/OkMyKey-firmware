#include <Keyboard.h>
#include <EEPROM.h>

//#define DEBUG

#define VERSION "1.0"

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
byte switches[numButtons] = {S1, S2, S3, S4, S5, S6, S7, S8};

//////////////////////////////////
///// Buttons Debounce
//////////////////////////////////
unsigned long debounceDelay = 125; // the debounce time in ms; increase if the output flickers
long minimumPressTime = 60; // in ms. increase to reduce phantom presses
unsigned long lastDebounceTimes[numButtons] = {};
unsigned long buttonPressedTimes[numButtons] = {};
bool lastButtonsStates[numButtons] = {};

unsigned long lastSerialTime = 0;

//////////////////////////////////
///// EEPROM Configuration
///// S1 0
///// +
///// S2 128
//////////////////////////////////
int address = 0;
const unsigned int reservedButtonMemory = EEPROM.length() / numButtons;
int buttonNum = 0;

//////////////////////////////////
///// Data decode settings
//////////////////////////////////
#define MAX_MACRO_ORDERS 5
#define MAX_COMMAND_LENGHT 127
#define PRESS_MODE '1'
#define PRINT_MODE '2'
#define SEPARATOR "|"

struct button_settings
{
  char raw[MAX_COMMAND_LENGHT * MAX_MACRO_ORDERS + 10];
  unsigned int buttonNumber;
  char mode;
  char commands[MAX_MACRO_ORDERS][MAX_COMMAND_LENGHT];
  unsigned int commandsNum;
};

void sendInfo()
{
  char infoStr[50];
  // Reserve 5 bytes for the header settings (button number, mode and separators)
  sprintf(infoStr, "Info: %d %s %d %d", numButtons, VERSION, MAX_COMMAND_LENGHT - 5 - MAX_MACRO_ORDERS, MAX_MACRO_ORDERS);
  Serial.println(infoStr);
}

//////////////////////////////////
///// Buttons Commands
//////////////////////////////////
void sendKeyPress(int key)
{
  // Retrieve the button settings
  char data[reservedButtonMemory];
  getButtonConfiguration(key, data);

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
    sprintf(debugString, "Button: %d, Mode: %c, Command: %x", key, settings.mode, settings.commands[i][0]);
    Serial.println(debugString);
#endif
    if (settings.mode == PRESS_MODE)
      Keyboard.press(settings.commands[i][0]);
    else if (settings.mode == PRINT_MODE)
      Keyboard.print(settings.commands[i]);
  }

  Keyboard.releaseAll();
}

void getButtonConfiguration(int buttonNumber, char *data)
{
  // Start address pointer
  unsigned int addressPointer = buttonNumber * reservedButtonMemory;

  // Read the string
  unsigned char readChar;
  const unsigned char dataLenght = EEPROM.read(addressPointer++);
  for (unsigned int len = 0; len < dataLenght; len++)
  {
    readChar = EEPROM.read(addressPointer + len);
    data[len] = readChar;
  }
  data[dataLenght] = '\0';
}

void setButtonConfiguration(int buttonNumber, const char *data)
{
  // Start address pointer
  unsigned int addressPointer = buttonNumber * reservedButtonMemory;

  unsigned int dataLenght = strlen(data);
  EEPROM.write(addressPointer++, dataLenght);
  for (int addrPos = 0; addrPos < dataLenght; addrPos++)
  {
    EEPROM.write(addressPointer + addrPos, data[addrPos]);
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
  char buttonKey[1];
  for (int i = 0; i < numButtons; i++)
  {
    sprintf(buttonKey, "%d|%c|%d", i, PRESS_MODE, i + 1);
    setButtonConfiguration(i, buttonKey);
  }
  Serial.println("Settings initialized.");
}

void setup()
{
  for (int i = 0; i < numButtons; i++)
  {
    pinMode(switches[i], INPUT);
    digitalWrite(switches[i], HIGH);
  }
  Keyboard.begin();
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
  if (errCheck == pch || *errCheck != '\0' || decodedSettings->buttonNumber < 0 || decodedSettings->buttonNumber >= numButtons)
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
    unsigned int commandLenght = strlen(pch);
#ifdef DEBUG
    Serial.print("Command: ");
    Serial.println(pch);
#endif
    if (commandLenght > MAX_COMMAND_LENGHT)
      return false;
    strcpy(decodedSettings->commands[commandsNum++], pch);
    pch = strtok(NULL, SEPARATOR);
  }

  decodedSettings->commandsNum = commandsNum;

  return commandsNum > 0;
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
  if (errCheck == pch || *errCheck != '\0' || buttonNumber < 0 || buttonNumber >= numButtons)
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

void loop()
{
  unsigned long currentMillis = millis();
  for (int i = 0; i < numButtons; i++)
  {
    // Debounce the button
    if ((currentMillis - lastDebounceTimes[i]) > debounceDelay)
    {
      bool currentState = digitalRead(switches[i]);
      // Rissing edge. Save press time
      if (currentState && lastButtonsStates[i] != currentState)
      {
        buttonPressedTimes[i] = currentMillis;
      }
      // Send the key press if it has been passed more than [minimumPressTime]ms with the button pressed
      if (currentState && (int)(currentMillis - buttonPressedTimes[i]) > minimumPressTime)
      {
        sendKeyPress(i);     
        buttonPressedTimes[i] = currentMillis + 10000; // Do not resend the order for 10000ms if the key keeps pressed
      }
      // Falling edge. Save debounce time
      if (!currentState && lastButtonsStates[i] != currentState)
      {
        lastDebounceTimes[i] = currentMillis;
      }
      lastButtonsStates[i] = currentState;
    }
  }

  recvData();
}