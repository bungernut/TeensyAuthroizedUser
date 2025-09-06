/*
Platform: teensy 4.1 w/ ethernet; keypad; TFT-screen
Code plan:
1) Store user codes in EEPROM (addresses 1-50) - codes are 4 numbers?
2) Store logs in EEPROM (addresses 100+) - do we want timestamp? hours since 1970? NTP? (TODO)
Teensy EEPROM docs: https://www.pjrc.com/teensy/td_libs_EEPROM.html


When program loads, read codes from EEPROM and store into MODBUS-registers (0-99)
Update codes by writing to register new user code
Modbus-register 0 - 1 writes codes to EEPROM, after write resets to 0
Modbus resister 50+ - some kind of log that can be pulled by client2database app? Do we want to clear it or round-robbin? (TODO)
*/
#include <Wire.h>
#include <SPI.h>
#include <NativeEthernet.h>
#include <NativeEthernetUdp.h>
#include <NTPClient.h>
#include <EEPROM.h>
#include <ArduinoRS485.h> // ArduinoModbus depends on the ArduinoRS485 library
#include <ArduinoModbus.h>
#include <Keypad.h>

// From NativeEthernet UDPNTP Example
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
EthernetUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // NTP server, GMT offset (seconds), update interval (milliseconds)

// Modbus 
EthernetServer ethServer(502);
EthernetClient client;
ModbusTCPServer modbusTCPServer;
const double updateModbusMillis = 200;
double lastModbusMillis=0;

// EEPROM Variables
uint8_t highByte = 0x12; // Example high byte
uint8_t lowByte = 0x34;  // Example low byte
uint16_t combinedValue;

// Keypad setup
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] =
{
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {40, 39, 38, 37}; //row pins pins 0-3 on keypad
byte colPins[COLS] = {36, 35, 34, 33}; //column pins 4-7 on keypad
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

// Code globals
const String CORRECT_PASSWORD = "1234#"; // Example password ending with #
String enteredPassword = "";


void setup()
{
  Serial.begin(115200);
  // Ethernet + UDP ************************************
  // Need some non-blocking ethernet setup (TODO)
    // start Ethernet and UDP
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    } else if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
    // no point in carrying on, so do nothing forevermore:
    while (true) {
      delay(10);
    }
  }
  Serial.print("Arduino IP address: ");
  Serial.println(Ethernet.localIP());
  ntpUDP.begin(8888);

  // NTP *********************
  timeClient.begin();
  timeClient.update();

  // EEPROM
  Serial.println("Save1x EEPROM code...");
  EEPROM.write(0, 4);
  EEPROM.write(1, 210);
  Serial.println("DONE writing EEPROM");

  // Setup Modbus 
  modbusTCPServer.begin();
  modbusTCPServer.configureHoldingRegisters(0, 50);
  modbusTCPServer.holdingRegisterWrite(0, 0);
  for (int i=0; i<50;i++) {
    Serial.print(i);
    highByte = EEPROM.read(i*2);
    lowByte  = EEPROM.read(i*2+1);
    combinedValue = ((uint16_t)highByte << 8) | lowByte;
    modbusTCPServer.holdingRegisterWrite(i+1, combinedValue);
  }
  Serial.println("Done reading in EEPROM");
}

// **  LOOP ** LOOP ** LOOP ** //
double currentMillis = 0;
void loop()
{
  currentMillis = millis();

  char key = keypad.getKey();
  if (isDigit(key))
  {
    enteredPassword += key;
    Serial.print("key : ");
    Serial.print(enteredPassword);
    Serial.print(" @ ");
    Serial.println(timeClient.getFormattedTime());
  }
  if (key=='#') 
  {
    // Check password is correct?
    uint16_t code = enteredPassword.toInt();
    Serial.println(code);
    enteredPassword="";
    if (checkPassword(code)) {
      Serial.println("Good Password!");
    }
  }
  if (currentMillis - lastModbusMillis > updateModbusMillis) {
    client = ethServer.available();
    modbusTCPServer.accept(client);
    modbusTCPServer.poll();
    lastModbusMillis = currentMillis;
  }
}
// **  LOOP ** LOOP ** LOOP ** //


bool checkPassword(int code) {
  for (int i=0; i<50;i++) {
    highByte = EEPROM.read(i*2);
    lowByte  = EEPROM.read(i*2+1);
    combinedValue = ((uint16_t)highByte << 8) | lowByte;
    if (code == combinedValue && code != 0) {
      return true;
    }
  }
  return false;
}

uint16_t GetCodeEEPROM(uint8_t icode){
  highByte = EEPROM.read(icode*2);
  lowByte  = EEPROM.read(icode*2+1);
  combinedValue = ((uint16_t)highByte << 8) | lowByte;
}