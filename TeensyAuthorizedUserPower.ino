/*
Platform: teensy 4.1 w/ ethernet; keypad; TFT-screen
Teensy has 4kb of EEPROM
Code plan:
1) Store 20 user codes and names in EEPROM: codes are 4 numbers, names up to 10 char
2) Maybe Store last 10 logs in EEPROM w/ timestamp? as chars, so 10char+YYMMDDHHMM  = 20char, so start at 1000?
Teensy EEPROM docs: https://www.pjrc.com/teensy/td_libs_EEPROM.html


When program loads, read codes from EEPROM and store into array
Update codes via web interface?
Modbus-register 0 - 1 writes codes to EEPROM, after write resets to 0
Modbus resister 50+ - some kind of log that can be pulled by client2database app? Do we want to clear it or round-robbin? (TODO)
*/
#include <Wire.h>
#include <SPI.h>
//#include <NativeEthernet.h>
// #include <NativeEthernetUdp.h>
#include <QNEthernet.h>
using namespace qindesign::network;
#include <EasyWebServer.h> // https://github.com/llelundberg/EasyWebServer/tree/master
#include <NTPClient.h>
#include <EEPROM.h>
#include <Keypad.h>

// From NativeEthernet UDPNTP Example
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
//qindesign::network::EthernetUDP ntpUDP;
//NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // NTP server, GMT offset (seconds), update interval (milliseconds)

// EEPROM Variables
uint8_t highByte = 0x12; // Example high byte
uint8_t lowByte = 0x34;  // Example low byte
uint16_t combinedValue;
char charlog[20];

char c;
EthernetServer server(80);

void handleRoot(EasyWebServer &w) {
  w.client.println(F("<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head>"));
  //w.client.println(F("<link rel=\"icon\" href=\"data:,\">"));
  w.client.println(F("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center; font-size: 20px}</style></head>"));
  //w.client.println(F(".button { background-color: #4CAF50; border: none; color: white; padding: 12px 20px; border-radius: 8px; text-decoration: none; font-size: 20px; margin: 2px; cursor: pointer;}"));
  w.client.println(F("<body><h1>PM727 Log</h1>"));
  w.client.println(F("<p>"));
  for (int i=0; i<10; i++) {
    for (int j=0; j<20; j++){
      c = EEPROM.read(1000 + i*20 + j);
      w.client.print(c);
    }
    w.client.println(F("<br>"));
  }
  w.client.println(F("</p>"));
  w.client.println(F("</body></html>"));
  //server.send(200, "text/html", html);
}
void handleAuthorizedUsers(EasyWebServer &w){
  String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  html += "<link rel=\"icon\" href=\"data:,\">";
  html += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center; font-size: 20px}";
  html += ".button { background-color: #4CAF50; border: none; color: white; padding: 12px 20px; \
                     border-radius: 8px; text-decoration: none; font-size: 20px; margin: 2px; cursor: pointer;}";
  html += ".button2 { background-color: #555555; }</style></head>";
  html += "<body><h1>ESPBrew</h1>";
  html += "<p>";
  
  html += "</body></html>";
  w.client.println(html);
}


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
  //ntpUDP.begin(8888);

  Serial.print("NTP...");
  // NTP *********************
  //timeClient.begin();
  //timeClient.update();
  Serial.println(" done");

  // EEPROM
  Serial.println("Save1x EEPROM code...");
  String exampleLog = "bmong    2509060606";
  exampleLog.toCharArray(charlog,20);
  EEPROM.put(1000, charlog);
  exampleLog = "sluitz   2509060606";
  exampleLog.toCharArray(charlog,20);
  EEPROM.put(1020, charlog);
  Serial.println("DONE writing EEPROM");

  Serial.println("Setup WebServer");
  MDNS.begin("myteensy");
  MDNS.addService("_http", "_tcp", 80);
  
  server.begin();
  Serial.print("server is at ");
  Serial.println(Ethernet.localIP());
  Serial.println("Done WebServer");
}

// **  LOOP ** LOOP ** LOOP ** //
double currentMillis = 0;
char key;
void loop()
{
  currentMillis = millis();

  key = keypad.getKey();
  if (isDigit(key))
  {
    enteredPassword += key;
    Serial.print("key : ");
    Serial.print(enteredPassword);
    Serial.print(" @ ");
    //Serial.println(timeClient.getFormattedTime());
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
  //server.handleClient();
  EthernetClient client = server.available();
  if (client) { // If a client is connected
    Serial.println("New client!");
    EasyWebServer w(client);
    Serial.println(w.url);
    // This dies indeed print the url passed so we can add users maybe this way?
    w.serveUrl("/",handleRoot);  
    w.serveUrl("/authorizedUsers",handleAuthorizedUsers);
  }

}
// **  LOOP ** LOOP ** LOOP ** //


uint16_t GetCodeEEPROM(uint8_t icode){
  highByte = EEPROM.read(icode*2);
  lowByte  = EEPROM.read(icode*2+1);
  combinedValue = ((uint16_t)highByte << 8) | lowByte;
}