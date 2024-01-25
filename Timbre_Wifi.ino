// BEGIN CONFIG ///////////////////////////////////////////////////////////////////////////
#define MIC_ENABLED true
#define BONJOUR false
#define OTA_UPDATE false

// END CONFIG //////////////////////////////////////////////////////////////////////////////

#include <ESP8266WiFi.h>
#include "WifiPass.h"  //define wifi SSID & pass
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>

#if BONJOUR 
#include <ESP8266mDNS.h>
#endif

#if OTA_UPDATE
#include <ArduinoOTA.h>
#endif


// GLOBALS /////////////////////////////////////////////////////////////////////////////////
float loudness = 0.0f;
int sleepMS = 16;
float loudnessThreshold = 0.8;
int loudCounter = 0;
bool debugPrint = false;
float alertFilter = 0;

ESP8266WebServer server(80);

//global devices
String ID;

// WIFI ////////////////////////////////////////////////////////////////////////////////////

// RESET
void (*resetFunc)(void) = 0;  //declare reset function at address 0

void handleRoot() {
  static char json[200];
  sprintf(json, "{\"ID\":\"%s\", \"loud\":%f, \"alertness\":%f }", ID, loudness, alertFilter);
  server.send(200, "application/json", json);
}

void handleEnableDebug(){
  server.send(200, "text/html", "OK");
  debugPrint ^= true;
}

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);    //Initialize the LED_BUILTIN pin as an output
  digitalWrite(LED_BUILTIN, LOW);  //turn on red led
  pinMode(A0, INPUT);

  Serial.begin(19200);
  Serial.println("----------------------------------------------\n");
  //ID = String(ESP.getChipId(), HEX);
  ID = "Doorbell";

  //WiFi.setPhyMode(WIFI_PHY_MODE_11G);
  //WiFi.mode(WIFI_OFF);    //otherwise the module will not reconnect
  WiFi.mode(WIFI_STA);  //if it gets disconnected
  WiFi.disconnect();

  WiFi.begin(ssid, password);
  Serial.printf("Trying to connect to %s ...\n", ssid);
  int wifiAtempts = 0;
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    wifiAtempts++;
    if (wifiAtempts > 10) {
      Serial.printf("\nFailed to connect! Rebooting!\n");
      digitalWrite(LED_BUILTIN, HIGH);  //turn off red led
      ESP.restart();
    }
  }
  digitalWrite(LED_BUILTIN, HIGH);  //turn off red led
  Serial.printf("\nConnected to %s IP addres %s\n", ssid, WiFi.localIP().toString().c_str());

  WiFi.setAutoReconnect(true);
  WiFi.setSleepMode(WIFI_LIGHT_SLEEP);
  

  server.on("/", handleRoot);
  server.on("/debugOn", handleEnableDebug);
  server.begin();
  Serial.println("HTTP server started");

#if BONJOUR
  if (MDNS.begin(ID)) {
    Serial.println("MDNS responder started " + ID);
  }
  MDNS.addService("http", "tcp", 80);   // Add service to MDNS-SD
#endif

#if OTA_UPDATE
  setupOTA();
#endif

  updateSensorData();
}


void loop() {

  delay(sleepMS);  //once per second
  #if BONJOUR
    MDNS.update();
  #endif
  
  server.handleClient();
  updateSensorData();

  if(loudness > loudnessThreshold && alertFilter < 0.1){    
    loudCounter ++;
    Serial.printf("loud! (loudCounter %d)\n", loudCounter);
    if(loudCounter > 2){
      Serial.printf("notify!!!\n");
      alertFilter = 1;
      loudCounter = 0;
      
      WiFiClient client;
      HTTPClient http;
      digitalWrite(LED_BUILTIN, LOW);  //turn ON led
      if (http.begin(client, "http://10.0.0.10:12345/doorbell")) {  // HTTP
        int httpResponseCode = http.GET();
        http.end();
      } else {
        Serial.println("[HTTP] Unable to connect");
      }
      digitalWrite(LED_BUILTIN, HIGH);  //turn off led
    }
  }else{
    loudCounter = 0;
  }

  #if OTA_UPDATE
    ArduinoOTA.handle();
  #endif

  alertFilter *= 0.985;
  if(alertFilter > 0.1){
    Serial.printf("alertFilter: %f\n", alertFilter);
  }
}

void updateSensorData() {

#if (MIC_ENABLED)  //calc mic input gain //////////////////////
  int mn = 1024;
  int mx = 0;
  for (int i = 0; i < 512; ++i) {
    int val = analogRead(A0);
    mn = min(mn, val);
    mx = max(mx, val);
  }
  loudness = (mx - mn) / 1024.0f;  //note 2X gain to get a little more contrast
  if(debugPrint) Serial.printf("loud: %f\n", loudness);
#endif
}

///////////////// OTA UPDATES //////////////////////////////////////////////////////

#if OTA_UPDATE
void setupOTA() {
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });

  ArduinoOTA.begin();
}
#endif
