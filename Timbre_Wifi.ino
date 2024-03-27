// BEGIN CONFIG ///////////////////////////////////////////////////////////////////////////
#define HOST_NAME "Timbre"
#define SERVER_WWW true
// END CONFIG //////////////////////////////////////////////////////////////////////////////

#include <ESP8266WiFi.h>
#include "WifiPass.h"	//define wifi SSID & pass
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>

// GLOBALS /////////////////////////////////////////////////////////////////////////////////
float loudness = 0.0f;
int sleepMS = 5;
float loudnessThreshold = 0.40;
int loudConterTrigger = 1;
int loudCounter = 0;
int loudCounterMaxSoFar = 0;
float loudestSoFar = 0;
float alertFilter = 0;

#if SERVER_WWW
ESP8266WebServer server(80);
#endif

void updateSensorData();

// WIFI ////////////////////////////////////////////////////////////////////////////////////

#if SERVER_WWW
void handleRoot() {
	static char json[200];
	sprintf(json, "{\"ID\":\"%s\", \"loud\":%f, \"alertness\":%f, \"loudConterTrigger\":%d, \"loudCounterMaxSoFar\":%d, \"loudnessThreshold\":%f, \"loudestSoFar:\":%f }", HOST_NAME, loudness, alertFilter, loudConterTrigger, loudCounterMaxSoFar, loudnessThreshold, loudestSoFar);
	server.send(200, "application/json", json);
}


void handleTrigger() {
	alertFilter = 1;
	server.send(200, "text/plain", "OK");
}

void handleReset() {
	ESP.restart();
}

#endif


void setup() {
	pinMode(A0, INPUT);

	Serial.begin(9600);
	Serial.println("----------------------------------------------\n");

	WiFi.setPhyMode(WIFI_PHY_MODE_11G);
	WiFi.setSleepMode(WIFI_NONE_SLEEP);
	//WiFi.mode(WIFI_OFF);		//otherwise the module will not reconnect
	WiFi.mode(WIFI_STA);	//if it gets disconnected
	WiFi.disconnect();
	WiFi.setHostname(HOST_NAME);
	WiFi.begin(ssid, password);

	Serial.printf("Trying to connect to %s ...\n", ssid);
	if (WiFi.waitForConnectResult() != WL_CONNECTED) {
		delay(5000);
		ESP.restart();
	}
	Serial.printf("\nConnected to %s IP address %s\n", ssid, WiFi.localIP().toString().c_str());
	
#if SERVER_WWW
	server.on("/", handleRoot);
	server.on("/trigger", handleTrigger);
	server.on("/reset", handleReset);
	server.begin();
	Serial.println("HTTP server started");
#endif

	ArduinoOTA.setHostname(HOST_NAME);
	ArduinoOTA.setRebootOnSuccess(true);
	ArduinoOTA.begin();	

	Serial.println("ready!");
}


void loop() {

	ArduinoOTA.handle();
	updateSensorData();

	if(loudness > loudnessThreshold && alertFilter < 0.1){		
		loudCounter ++;
		loudCounterMaxSoFar = max(loudCounterMaxSoFar, loudCounter);
		Serial.printf("loud! (loudCounter %d)\n", loudCounter);
		if(loudCounter >= loudConterTrigger){
			Serial.printf("notify!!!\n");
			alertFilter = 1;
			loudCounter = 0;
			
			WiFiClient client;
			HTTPClient http;

			if (http.begin(client, "http://10.0.0.10:12345/doorbell")) {	// HTTP
				int httpResponseCode = http.GET();
				Serial.print("[HTTP] response code: ");
				Serial.println(httpResponseCode, 1);
				http.end();
			} else {
				Serial.println("[HTTP] Unable to connect");
			}
		}
	}else{
		loudCounter = 0;
	}

	#if SERVER_WWW
	server.handleClient();
	#endif

	alertFilter *= 0.98;

	delay(sleepMS);	//once per second
}


void updateSensorData() {

	int mn = 1024;
	int mx = 0;
	for (int i = 0; i < 64; ++i) {
		int val = analogRead(A0);
		mn = min(mn, val);
		mx = max(mx, val);
	}
	loudness = (mx - mn) / 1024.0f;
	loudestSoFar = max(loudestSoFar, loudness);
	//Serial.printf("loud:%f\n", loudness);
}
