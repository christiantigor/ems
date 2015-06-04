#include <SoftwareSerial.h>
#include <espduino.h>
#include <rest.h>

SoftwareSerial debugPort(2,3);    //rx, tx
ESP esp(&Serial, &debugPort, 4);  //ch_pd
REST rest(&esp);
boolean wifiConnected = false;

void wifiCb(void* response) {
  uint32_t status;
  RESPONSE res(response);
  
  if(res.getArgc() == 1) {
    res.popArgs((uint8_t*)&status, 4);
    if(status == STATION_GOT_IP) {
      debugPort.println("WIFI CONNECTED");
      wifiConnected = true;
    }
    else {
      wifiConnected = false;
    }
  }
}

void setup() {
  Serial.begin(19200);
  debugPort.begin(19200);
  esp.enable();
  delay(500);
  esp.reset();
  delay(500);
  while(!esp.ready());
  
  debugPort.println("ARDUINO: setup rest client");
  if(!rest.begin("api.thingspeak.com")) {
    debugPort.println("ARDUINO: failed to setup rest client");
    while(1);
  }
  
  debugPort.println("ARDUINO: setup wifi");
  esp.wifiCb.attach(&wifiCb);
  esp.wifiConnect("Belkin_G_Plus_MIMO","");
  debugPort.println("ARDUINO: system started");
}

void loop() {
  char response[255];
  esp.process();
  if(wifiConnected) {
    rest.get("/talkbacks/887/commands/last?api_key=ACM8XW24UDVY1GTV");
    if(rest.getResponse(response, 255) == HTTP_STATUS_OK) {
      debugPort.println("RESPONSE: ");
      debugPort.println(response);
    }
    delay(1000);
  }
}
