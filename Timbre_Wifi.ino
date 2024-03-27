// BEGIN CONFIG ///////////////////////////////////////////////////////////////////////////
#define HOST_NAME "Timbre"
// END CONFIG //////////////////////////////////////////////////////////////////////////////

#include "WifiPass.h"	//define wifi SSID & pass
#include <SerialWebLog.h>
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

SerialWebLog mylog;

void updateSensorData();

// WIFI ////////////////////////////////////////////////////////////////////////////////////

void handleState() {
	static char json[200];
	sprintf(json, "{\"ID\":\"%s\", \"loud\":%f, \"alertness\":%f, \"loudConterTrigger\":%d, \"loudCounterMaxSoFar\":%d, \"loudnessThreshold\":%f, \"loudestSoFar:\":%f }", HOST_NAME, loudness, alertFilter, loudConterTrigger, loudCounterMaxSoFar, loudnessThreshold, loudestSoFar);
	mylog.getServer()->send(200, "application/json", json);
}

void handleTrigger() {
	alertFilter = 1;
	mylog.getServer()->send(200, "text/plain", "OK");
}


void setup() {

	pinMode(A0, INPUT);

	mylog.setup(HOST_NAME, ssid, password);

	//add an extra endpoint
	mylog.getServer()->on("/trigger", handleTrigger);
	mylog.addHtmlExtraMenuOption("Trigger", "/trigger");	

	mylog.getServer()->on("/state", handleState);
	mylog.addHtmlExtraMenuOption("State", "/state");	

	ArduinoOTA.setHostname(HOST_NAME);
	ArduinoOTA.setRebootOnSuccess(true);
	ArduinoOTA.begin();	

	mylog.print("ready!\n");
}


void loop() {

	ArduinoOTA.handle();
	updateSensorData();
	mylog.update();

	if(loudness > loudnessThreshold && alertFilter < 0.1){		
		loudCounter ++;
		loudCounterMaxSoFar = max(loudCounterMaxSoFar, loudCounter);
		mylog.printf("loud! (loudCounter %d)\n", loudCounter);
		if(loudCounter >= loudConterTrigger){
			mylog.print("notify!!!\n");
			alertFilter = 1;
			loudCounter = 0;
			
			WiFiClient client;
			HTTPClient http;

			if (http.begin(client, "http://10.0.0.10:12345/doorbell")) {	// HTTP
				mylog.printf("[HTTP] response code: %d\n", http.GET());
				http.end();
			} else {
				mylog.print("[HTTP] Unable to connect\n");
			}
		}
	}else{
		loudCounter = 0;
	}
	alertFilter *= 0.97;

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
	//mylog.printf("loud:%f\n", loudness);
}
