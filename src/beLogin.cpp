/**
 * Comment the next line for avoiding the Serial Library to be included in the compilation
 */
#define DEBUGGIN

/****************************
 * ESP principal Library
 ****************************/
#include "Arduino.h"
// #include "ESP8266WiFi.h"

/*****************************
 * Websockets Libraries
 ****************************/
#include <WebSocketsClient.h>
#include <Hash.h>

/****************************
 * RFID Libraries
 ****************************/
#include "SPI.h"
#include "MFRC522.h"

/*****************************
 * RTC Libraries
 *****************************/
#include "Wire.h"
#include "RtcDS1307.h"

/*****************************
 * Data codification library
 *****************************/
#include "ArduinoJson.h"

/*****************************
 * Auto Wifi configuration
 *****************************/
 #include <DNSServer.h>
 #include <ESP8266WebServer.h>
 #include <WiFiManager.h>


/**
 * Constants for the SPI protocol used by the RFID
 */
#define SS_PIN 15
#define RST_PIN 3
String newCard = "";

/**
 * Objets of the system
 */
//Use for Get the information of the cards
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
//USE RtcDS1307 for configure and manipulate the Real Tiny Clock
RtcDS1307 RTC;
//Use for storage the data that could'n be sent to the server

// Websocket Client
WebSocketsClient webSocket;
//Wifi Manager Object
WiFiManager wifiManager;


/**
 * Configuration variables for the Wifi Connection
 */
#define LISTEN_PORT 3000
#define TIME_OUT_CONNECTION_TIME_S 12000
const char* ssid     = "SSID";
const char* password = "PASSWORD";
char WebSocketServerIP[] = "192.168.1.35";

/**
 * Time Varibales
 */
uint8_t hour;
uint8_t minute;
uint8_t second;
uint8_t day;
uint8_t month;
uint16_t year;

/**
 * Definitions of configuration functions
 */
void setup_MFRC522();
void setup_DS1307();
bool setup_WiFi();
void setup_WebSocketConection();
void setup_SD();
void setup_WifiManager();

/**
 * Definitions of infinite funtions
 */
bool loop_MFRC522();
void loop_DS1307();
bool loop_WiFi();
void loop_SD();

/**
 * Data codification method
 */
void encodeData();
bool sendData();

void webSocketEvent(WStype_t type, uint8_t * payload, size_t lenght);

/**
 * Debuggin Functions
 */
#ifdef DEBUGGIN
  void printDec(byte *buffer, byte bufferSize);
  void printHex(byte *buffer, byte bufferSize);
  void printDateTime(const RtcDateTime& dt);
#endif



void setup() {
  #ifdef DEBUGGIN
    Serial.begin(9600);
  #endif
  setup_WifiManager();
  setup_WiFi();
  setup_WebSocketConection();
  setup_MFRC522();
  setup_DS1307();

}

void loop() {
  webSocket.loop();
  if (loop_MFRC522()) {
    loop_DS1307();
    encodeData();
    if (loop_WiFi()) {
      sendData();
    }else{

    }
  }
}

/**
* Configuration of MFRC522
*/
void setup_MFRC522() {
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
  #ifdef DEBUGGIN
    Serial.println("RFID Initialized");
  #endif

  rfid.PCD_SetAntennaGain(MFRC522::PCD_RxGain::RxGain_max);
  #ifdef DEBUGGIN
    Serial.println("Antenna Gain set");
  #endif
}

/**
* Configuration of DS1307
*/
void setup_DS1307() {
  RTC.Begin(); // Init Real Time Clock
  #ifdef DEBUGGIN
    Serial.println("RTC was Initilized");
  #endif
  //RTC.SetDateTime(RtcDateTime(__DATE__,__TIME__));//Comment this line once RTC was configured
  //
  // RTC.SetDateTime(
  //   RtcDateTime(
  //     2016,//Year
  //     4,//Month
  //     26,//dayOfMonth
  //     21,//Hour
  //     48,//Minute
  //     0//Second
  //   ));
  if (!RTC.GetIsRunning()){
    #ifdef DEBUGGIN
      Serial.println("RTC was not running, starting now");
    #endif
    RTC.SetIsRunning(true);
  }
}

/**
 * Connection of the Wifi
 */
bool setup_WiFi() {

  unsigned long previusTime = millis();
  bool setupWiFi = true;

  #ifdef DEBUGGIN

    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

  #endif

  WiFi.begin(ssid, password);

  do{
      #ifdef DEBUGGIN
        delay(200);
        Serial.print(".");
        // Serial.println(String(millis() - previusTime));
      #endif
      //if the connections fail prompt the message inside the "if" statement
      if (millis() - previusTime > TIME_OUT_CONNECTION_TIME_S){// If the status is diferent from the  8 seconds
        #ifdef DEBUGGIN
          Serial.println("Attemp connection timed out");
        #endif
        setupWiFi = false;
        break;
      }
  }while(WiFi.status() != WL_CONNECTED);

  if (setupWiFi != false) {

    #ifdef DEBUGGIN

      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());

    #endif

  }

  return setupWiFi;
}

/**
 * Setup Wifi Manager
 */
void setup_WifiManager(){
  wifiManager.autoConnect();
}
/**
 * Initializing the Web Socket connection
 * @return [description]
 */
void setup_WebSocketConection(){
  webSocket.begin(WebSocketServerIP, LISTEN_PORT);
  webSocket.onEvent(webSocketEvent);
}

/**
* Loop of the MFRC522
*/
bool loop_MFRC522() {

  //It will notify if there was a reading from the RFID
  bool wasRead = false;
  // Look for new cards
  if ( ! rfid.PICC_IsNewCardPresent())
  return wasRead;

  // Verify if the NUID has been readed
  if ( ! rfid.PICC_ReadCardSerial())
  return wasRead;

  #ifdef DEBUGGIN
    //Just print the card's footprint via serial
    Serial.print(F("PICC type: "));
    MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
    Serial.println(rfid.PICC_GetTypeName(piccType));

    Serial.println(F("The UID tag is:"));
    Serial.print(F("In hex: "));
    printHex(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
    Serial.print(F("In dec: "));
    printDec(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();

  #endif

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
  wasRead = true;
  return wasRead;
}

/**
 * Encode all the variables that the server needs as a JSON Object
 */
void encodeData() {
  //Json Object for communicating with the server
  StaticJsonBuffer<200> jsonBuffer;

  newCard = "";//Variable which is going to be send
  JsonObject& card = jsonBuffer.createObject();//Name of the JSON object
  JsonArray& code = card.createNestedArray("code");//Create an Array inside the JSONobject
  for (uint8_t i = 0; i < rfid.uid.size; i++) {//Add the card's footprint to the JSON object
    code.add(rfid.uid.uidByte[i]);
  }

  JsonObject& Time = jsonBuffer.createObject();
  //Add the Time variables
  Time["year"] = year;
  Time["month"] = month;
  Time["day"] = day;
  Time["hour"] = hour;
  Time["minute"] = minute;
  Time["second"] = second;

  card["time"] = Time;

  //Convert the data into a String
  card.printTo(newCard);
}

/**
* Loop of the DS1307
*/
void loop_DS1307() {
  hour    = RTC.GetDateTime().Hour();
  minute  = RTC.GetDateTime().Minute();
  second  = RTC.GetDateTime().Second();
  year    = RTC.GetDateTime().Year();
  month   = RTC.GetDateTime().Month();
  day     = RTC.GetDateTime().Day();
  #ifdef DEBUGGIN
    printDateTime(RTC.GetDateTime());
  #endif
}

/**
 * Verifies the connection to the WifI
 */
bool loop_WiFi(){

  bool loop_wifi = true;
  if (WiFi.status() != WL_CONNECTED) {

    #ifdef DEBUGGIN
      Serial.println("Client disconnected.");
    #endif

    loop_wifi = false;

    WiFi.begin(ssid,password);
  }
  return loop_wifi;
}

/**
 * Send Data to the Node Js Server
 */
bool sendData(){
  #ifdef DEBUGGIN
    Serial.println("Sending Data....");
  #endif
  /**
  * Send the card info
  */
  webSocket.sendTXT(newCard);
  #ifdef DEBUGGIN
    Serial.println("JSON: " + newCard);
  #endif
}

/**
 * WebSocket Event, it refers to send,receive(bin,txt) data
 *
 */
void webSocketEvent(WStype_t type, uint8_t * payload, size_t lenght) {


  switch(type) {

    case WStype_DISCONNECTED:
        Serial.printf("[WSc] Disconnected!\n");
        webSocket.begin("192.168.1.35", 3000);

    break;

    case WStype_CONNECTED:

        Serial.printf("[WSc] Connected to url: %s\n",  payload);

        // send message to server when Connected
        webSocket.sendTXT("Connected");

    break;

    case WStype_TEXT:
        Serial.printf("[WSc] get text: %s\n", payload);

        // send message to server
			  // webSocket.sendTXT("message here");

    break;

    case WStype_BIN:
        Serial.printf("[WSc] get binary lenght: %u\n", lenght);
        hexdump(payload, lenght);

        // send data to server
        // webSocket.sendBIN(payload, lenght);
    break;
  }

}

/**
 * Debuggin Functions
 */
#ifdef DEBUGGIN
  /**
  * Helper routine to dump a byte array as hex values to Serial.
  */
  void printHex(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
  }

  /**
  * Helper routine to dump a byte array as dec values to Serial.
  */
  void printDec(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], DEC);
    }
  }

  /**
  * Print the Time as a String
  */
  #define countof(a) (sizeof(a) / sizeof(a[0]))

  void printDateTime(const RtcDateTime& dt){

    char datestring[20];
    snprintf_P(datestring,
      countof(datestring),
      PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
      dt.Month(),
      dt.Day(),
      dt.Year(),
      dt.Hour(),
      dt.Minute(),
      dt.Second() );
      Serial.println(datestring);
    }
#endif
