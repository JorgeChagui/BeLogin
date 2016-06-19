  /**
   * Comment the next line for avoiding the Serial Library to be included in the compilation
   */
  #define DEBUGGIN
  #include "Arduino.h"
  #include "ESP8266WiFi.h"
  #include "SocketIOClient.h"
  #include "SPI.h"
  #include "MFRC522.h"
  #include "Wire.h"
  #include "RtcDS1307.h"
  #include "Arduinojson.h"


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

  // Socket IO client for communicating with NodeJs Server trhough a socket and TCP connections
  SocketIOClient client;



  /**
   * Configuration variables for the Wifi Connection
   */
  #define LISTEN_PORT 3000
  #define ALLOWED_PACKETS 4
  #define TIME_OUT_CONNECTION_TIME_S 12000
  #define WEBSOCKET_RECONECCTION_TIME_S 90000
  uint8_t packetsSent = 0;
  uint32_t timeFromLastPacketSent = 0;
  const char* ssid     = "TORIBIO***";
  const char* password = "11TO23ry62";
  char path[] = "/";
  char host[] = "192.168.43.234";

  unsigned long WiFiResetingTime = 0;
  bool STATUS_LOOPING_WIFI = true;
  bool WEB_SOCKET_CONNECTION = true;

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
  void setup_SD();

  /**
   * Definitions of infinite funtions
   */
  bool loop_MFRC522();
  void loop_DS1307();
  bool loop_WiFi();
  void loop_SD();

  /**
   * Will count the time for a reseting after get a fail response from the configuration of the wifi
   */
  void watchDog();
  void encodeData();
  bool connectWebSocket();

  /**
   * Debuggin Functions
   */
  #ifdef DEBUGGIN
    void printDec(byte *buffer, byte bufferSize);
    void printHex(byte *buffer, byte bufferSize);
    void printDateTime(const RtcDateTime& dt);
  #endif


  void setup() {
    WiFi.mode(WIFI_STA);
    #ifdef DEBUGGIN
      Serial.begin(9600);
    #endif

    SPI.begin(); // Init SPI bus
    if (setup_WiFi()){
      connectWebSocket();
    }
    setup_MFRC522();
    setup_DS1307();

  }

  void loop() {

    if (loop_MFRC522()) {
      loop_DS1307();
      encodeData();
      if (!loop_WiFi()) {
        /**
         * loop for the SD CARD saving routine
         */
      }else{
        connectWebSocket();
      }
    }
    // watchDog();
  }

  /**
  * Configuration of MFRC522
  */
  void setup_MFRC522() {

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
    RTC.SetDateTime(RtcDateTime(__DATE__,__TIME__));//Comment this line once RTC was configured
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
   * WebSocket Connection
   */
  bool connectWebSocket() {
    // Connect to the websocket server
    WEB_SOCKET_CONNECTION = true;

    if (client.connect(host, LISTEN_PORT)) {

      #ifdef DEBUGGIN
        Serial.println("Connected");
      #endif
      client.send("connection", "message", "Connected !!!!");

    } else {

      #ifdef DEBUGGIN
        Serial.println("Client Connection failed.");
      #endif

      WEB_SOCKET_CONNECTION = false;

    }

    return WEB_SOCKET_CONNECTION;
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
   * Loop of the Wifi function
   */
  bool loop_WiFi(){

    bool loop_wifi = true;
    if (client.connected()) {

      #ifdef DEBUGGIN
        Serial.println("Sending Data....");
      #endif
      /**
      * Send the card info
      */
      // client.reconnect(host , LISTEN_PORT);
      client.sendJSON("newcard", newCard);
      timeFromLastPacketSent = millis();
      packetsSent++;
    } else {

      #ifdef DEBUGGIN
        Serial.println("Client disconnected.");
      #endif

      loop_wifi = false;

    }

    if (!loop_wifi) {
      WiFiResetingTime = millis();
    }

    STATUS_LOOPING_WIFI = loop_wifi;
    return loop_wifi;
  }


  void watchDog() {

    if (STATUS_LOOPING_WIFI) {
      #ifdef DEBUGGIN
        Serial.println("Wifi reconfiguration routine initilized...!!");
      #endif
      uint16_t realtime = millis();
      if (millis() - WiFiResetingTime > watchDogInterval  ) {
        if (setup_WiFi()) {
          STATUS_LOOPING_WIFI = false;
          #ifdef DEBUGGIN
            Serial.println("Wifi reconfigured..!!");
          #endif
        }else{
          WiFiResetingTime = millis();
        }
      }
    }
    if(packetsSent == ALLOWED_PACKETS || (millis() - timeFromLastPacketSent > WEBSOCKET_RECONECCTION_TIME_S)){
      #ifdef DEBUGGIN
        Serial.println("Reconnecting the webSocket");
      #endif
      client.reconnect(host, LISTEN_PORT);
      packetsSent = 0;
      timeFromLastPacketSent = millis();
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
