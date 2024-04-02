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
float loudnessThreshold = 0.50;
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
	sprintf(json, "ID:%s\nloud:%f\nalertness:%f\nloudConterTrigger:%d\nloudCounterMaxSoFar:%d\nloudnessThreshold:%f\nloudestSoFar:%f", 
			HOST_NAME, loudness, alertFilter, loudConterTrigger, loudCounterMaxSoFar, loudnessThreshold, loudestSoFar);
	mylog.getServer()->send(200, "text/plain", json);
}

void handleTrigger() {
	alertFilter = 0.0;
	loudness = loudnessThreshold;
	loudCounter = loudConterTrigger;
	mylog.print("manual trigger!\n");
	mylog.getServer()->send(200, "text/plain", "Triggered OK!");
}


void setup() {

	pinMode(A0, INPUT);

	uint8_t bssid[6] = {0x60,0x8d,0x26,0xeb,0xe2,0x8b}; //60:8d:26:eb:e2:8b Orangebox6
	//uint8_t bssid[6] = {0x04,0x20,0x84,0x34,0xe9,0x8b}; //04:20:84:34:e9:8b zte extender
	
	mylog.setup(HOST_NAME, ssid, password, bssid);

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

	if(loudness >= loudnessThreshold && alertFilter < 0.1){		
		loudCounter ++;
		loudCounterMaxSoFar = max(loudCounterMaxSoFar, loudCounter);
		mylog.printf("loud! (loudCounter %d)\n", loudCounter);
		if(loudCounter >= loudConterTrigger){
			mylog.print("Notify server!!!\n");
			alertFilter = 1;
			loudCounter = 0;
			
			WiFiClient client;
			HTTPClient http;

			if (http.begin(client, "http://10.0.0.10:12345/doorbell")) {	// HTTP
				mylog.printf("[HTTP] Response Code: %d\n", http.GET());
				http.end();
			} else {
				mylog.print("[HTTP] Unable to connect!\n");
			}
		}
	}else{
		loudCounter = 0;
	}

	//handle too many consecutive beeps filter
	alertFilter *= 0.995;
	if(alertFilter < 0.1 && alertFilter > 0.01){
		alertFilter = 0;
		mylog.print("Ready for more Beeps!\n");
	}

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
