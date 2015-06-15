#include <SoftwareSerial.h>
#include <espduino.h>
#include <mqtt.h>

SoftwareSerial DBG(2,3);    //rx, tx
ESP esp(&Serial, &DBG, 4);  //ch_pd
MQTT mqtt(&esp);
boolean wifiConnected = false;
char s[20];
String str;
int i;

void wifiCb(void* response) {
  uint32_t status;
  RESPONSE res(response);
  
  if(res.getArgc() == 1) {
    res.popArgs((uint8_t*)&status,4);
    if(status == STATION_GOT_IP) {
      DBG.println("WIFI CONNECTED");
      mqtt.connect("q.thingfabric.com",1883,false);
      wifiConnected = true;
    }
    else {
      wifiConnected = false;
      mqtt.disconnect();
    }
  }
}

void mqttConnected(void* response){
  DBG.println("Connected");
  mqtt.subscribe("gaisbwqreqrfxjc/066eff575349896767073038/command");
  //mqtt.subscribe("gaisbwqreqrfxjc/066eff575349896767073038/panelTemp");
  //mqtt.subscribe("gaisbwqreqrfxjc/066eff575349896767073038/panelHum");
  mqtt.publish("gaisbwqreqrfxjc/066eff575349896767073038/panelTemp","{\"time\":\"20150609-12:32:12\",\"value\":12.34}");
}

void mqttDisconnected(void* response){
  DBG.println("Disconnected");
}

void mqttData(void* response){
  RESPONSE res(response);
  
  DBG.print("Received: topic=");
  String topic = res.popString();
  DBG.println(topic);
  
  DBG.print("data=");
  String data = res.popString();
  DBG.println(data);
}

void mqttPublished(void* response){
}

void setup() {
  Serial.begin(19200);
  DBG.begin(19200);
  esp.enable();
  delay(500);
  esp.reset();
  delay(500);
  while(!esp.ready());
  
  DBG.println("ARDUINO: setup mqtt client");
  if(!mqtt.begin("emma/066eff575349896767073038", "761b233e-a49a-4830-a8ae-87cec3dc1086", "ef25cf4567fbc07113252f8d72b7faf2", 120, 1)) {
    DBG.println("ARDUINO: fail to setup mqtt");
    while(1);
  }
  
  DBG.println("ARDUINO: setup mqtt lwt");
  mqtt.lwt("/lwt", "offline", 0, 0); //or mqtt.lwt("/lwt", "offline");
  
  mqtt.connectedCb.attach(&mqttConnected);
  mqtt.disconnectedCb.attach(&mqttDisconnected);
  mqtt.publishedCb.attach(&mqttPublished);
  mqtt.dataCb.attach(&mqttData);
  
  DBG.println("ARDUINO: setup wifi");
  esp.wifiCb.attach(&wifiCb);
  
  esp.wifiConnect("Belkin_G_Plus_MIMO","");
  
  DBG.println("ARDUINO: system started");
}

void loop() {
  esp.process();
  if(wifiConnected) {
//    str = "{\"kirim\":\"data"+ String(i) +"!\"}";  
//    str.toCharArray(s,20);
//    DBG.println(s);
//    mqtt.publish("2da0575fc6dacf4205ce8174e7b3163b/topic/0",s);
//    i++;
  }
}
