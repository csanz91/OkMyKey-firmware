# OkMyKey-firmware
Macro keyboard firmware for an Arduino Pro Micro

This firmware controls the OkMyKey macro keyboard. 
Supports multiple keys, each key configuration is saved on the EEPROM.

# First time
The first time you use this firmware you need to set it up. To do so, you must send the following commands via the serial port:
## 1. Clear the EEPROM memory
  Send the command **"clear"**
    
  If successful you will see the text: "EEPROM cleared."
  
### 2. Init the buttons configuration

  Send the command **"init"**
    
  If successful you will see the text: "Settings initialized."
  
  
From now on, you can continue using the desktop application to configure the keyboard. To manually configure you can continue reading.

# Manual configuration
To configure a button, you need to send a command with the following pattern:

  > N|M|D
  
  Being:
  
    N: button number (int), starts from 0.
    M: Button action mode (int), 1: Press mode, to send a key combination; 2 Print mode, to send a string.
    D: Data (str). The string or keys combination separated with the '+' char.
    
  Examples:
  
   * **'0|1|1'** -> When the button number 0 is pressed the key '1' will be sent.
   * **'7|1|Â'** -> When the button number 8 is pressed the key 'F1' will be sent. The special keys codes, need to be converted to chars.
   * **'5|2|this text will be typed'** -> When the button number 6 is pressed the string 'this text will be typed' will be typed.
    
# Retrieve configuration
To get the stored configuration for a key, you can use the command **"getSettings N"**, 'N' is the button number, starting with 0.

It will return a string with the following format: **"button settings: N|M|D"**. It follows the same pattern as the manual configuration.

# Device Info
To test the device or to retrieve the features supported by the keyboard, it is possible to send the command **"info"**. It will return the following string: **"Info: NB V S M"**

  Being:
  
    NB: the number of buttons configured in the keyboard.
    V: the firmware version
    S: maximum data length. i.e.: the length of the data that can be stored for each button.
    M: maximum macro orders. The maximum number of key combinations for each button.
    
# EasyEDA
  https://easyeda.com/cesarsanz.91/macro
