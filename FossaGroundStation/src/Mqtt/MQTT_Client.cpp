/*
  MQTTClient.cpp - MQTT connection class
  
  Copyright (C) 2020 @G4lile0, @gmag12 and @dev_4m1g0

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "MQTT_Client.h"
#include "ArduinoJson.h"
#include "../Radio/Radio.h"

MQTT_Client::MQTT_Client() 
: PubSubClient(espClient)
{ }

void MQTT_Client::loop() {
  if (!connected() && millis() - lastConnectionAtempt > reconnectionInterval) {
    lastConnectionAtempt = millis();
    connectionAtempts++;
    status.mqtt_connected = false;
    reconnect();
  }
  else {
    connectionAtempts = 0;
    status.mqtt_connected = true;
  }

  if (connectionAtempts > connectionTimeout) {
    Serial.println("Unable to connect to MQTT Server after many atempts. Restarting...");
    ESP.restart();
  }

  PubSubClient::loop();

  unsigned long now = millis();
  if (now - lastPing > pingInterval) {
    lastPing = now;
    publish(buildTopic(topicPing).c_str(), "1");
  }
}

void MQTT_Client::reconnect() {
  ConfigManager& configManager = ConfigManager::getInstance();
  uint64_t chipId = ESP.getEfuseMac();
  char clientId[13];
  sprintf(clientId, "%04X%08X",(uint16_t)(chipId>>32), (uint32_t)chipId);

  Serial.print("Attempting MQTT connection...");
  Serial.println ("If this is taking more than expected, connect to the config panel on the ip: " + WiFi.localIP().toString() + " to review the MQTT connection credentials.");
  if (connect(clientId, configManager.getMqttUser(), configManager.getMqttPass(), buildTopic(topicStatus).c_str(), 2, false, "0")) {
    Serial.println("connected");
    subscribeToAll();
    sendWelcome();
  }
  else {
    Serial.print("failed, rc=");
    Serial.print(state());
  }
}

String MQTT_Client::buildTopic(const char* topic){
  ConfigManager& configManager = ConfigManager::getInstance();
  return String(topicStart) + "/" + String(configManager.getMqttUser()) + "/" + String(configManager.getThingName()) + "/" +  String(topic);
}

void MQTT_Client::subscribeToAll() {
  String sat_pos_oled = String(topicStart) + "/global/sat_pos_oled";
  subscribe(buildTopic(topicData).c_str());
  subscribe(sat_pos_oled.c_str());
}

void MQTT_Client::sendWelcome() {
  ConfigManager& configManager = ConfigManager::getInstance();
  const size_t capacity = JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(16);
  DynamicJsonDocument doc(capacity);
  doc["station"] = configManager.getThingName();
  JsonArray station_location = doc.createNestedArray("station_location");
  station_location.add(configManager.getLatitude());
  station_location.add(configManager.getLongitude());
  doc["version"] = status.version;
  doc["board"] = configManager.getBoard();

  serializeJson(doc, Serial);

  char buffer[512];
  size_t n = serializeJson(doc, buffer);
  publish(buildTopic(topicWelcome).c_str(), buffer,n );
}

void  MQTT_Client::sendSystemInfo() {
  ConfigManager& configManager = ConfigManager::getInstance();
  time_t now;
  time(&now);
  const size_t capacity = JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(19);
  DynamicJsonDocument doc(capacity);
  doc["station"] = configManager.getThingName();  // G4lile0
  JsonArray station_location = doc.createNestedArray("station_location");
  station_location.add(configManager.getLatitude());
  station_location.add(configManager.getLongitude());
  doc["rssi"] = status.lastPacketInfo.rssi;
  doc["snr"] = status.lastPacketInfo.snr;
  doc["frequency_error"] = status.lastPacketInfo.frequencyerror;
  doc["unix_GS_time"] = now;
  doc["batteryChargingVoltage"] = status.sysInfo.batteryChargingVoltage;
  doc["batteryChargingCurrent"] = status.sysInfo.batteryChargingCurrent;
  doc["batteryVoltage"] = status.sysInfo.batteryVoltage;
  doc["solarCellAVoltage"] = status.sysInfo.solarCellAVoltage;
  doc["solarCellBVoltage"] = status.sysInfo.solarCellBVoltage;
  doc["solarCellCVoltage"] = status.sysInfo.solarCellCVoltage;
  doc["batteryTemperature"] = status.sysInfo.batteryTemperature;
  doc["boardTemperature"] = status.sysInfo.boardTemperature;
  doc["mcuTemperature"] = status.sysInfo.mcuTemperature;
  doc["resetCounter"] = status.sysInfo.resetCounter;
  doc["powerConfig"] = status.sysInfo.powerConfig;
  serializeJson(doc, Serial);
  char buffer[512];
  serializeJson(doc, buffer);
  size_t n = serializeJson(doc, buffer);
  
  publish(buildTopic(topicSysInfo).c_str(), buffer,n );
}

void  MQTT_Client::sendPong() {
  ConfigManager& configManager = ConfigManager::getInstance();
  time_t now;
  time(&now);
  const size_t capacity = JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(7);
  DynamicJsonDocument doc(capacity);
  doc["station"] = configManager.getThingName();
  JsonArray station_location = doc.createNestedArray("station_location");
  station_location.add(configManager.getLatitude());
  station_location.add(configManager.getLongitude());
  doc["rssi"] = status.lastPacketInfo.rssi;
  doc["snr"] = status.lastPacketInfo.snr;
  doc["frequency_error"] = status.lastPacketInfo.frequencyerror;
  doc["unix_GS_time"] = now;
  doc["pong"] = 1;
  serializeJson(doc, Serial);
  char buffer[256];
  serializeJson(doc, buffer);
  size_t n = serializeJson(doc, buffer);

  publish(buildTopic(topicPong).c_str(), buffer, n);
}

void  MQTT_Client::sendMessage(char* frame, size_t respLen) {
  ConfigManager& configManager = ConfigManager::getInstance();
  time_t now;
  time(&now);
  Serial.println(String(respLen));
  char tmp[respLen+1];
  memcpy(tmp, frame, respLen);
  tmp[respLen-12] = '\0';

  // if special miniTTN message   
  Serial.println(String(frame[0]));
  Serial.println(String(frame[1]));
  Serial.println(String(frame[2]));
  // if ((frame[0]=='0x54') &&  (frame[1]=='0x30') && (frame[2]=='0x40'))
  if ((frame[0]=='T') &&  (frame[1]=='0') && (frame[2]=='@'))
  {
    Serial.println("mensaje miniTTN");
    const size_t capacity = JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(11) +JSON_ARRAY_SIZE(respLen-12);
    DynamicJsonDocument doc(capacity);
    doc["station"] = configManager.getThingName();
    JsonArray station_location = doc.createNestedArray("station_location");
    station_location.add(configManager.getLongitude());
    station_location.add(configManager.getLongitude());
    doc["rssi"] = status.lastPacketInfo.rssi;
    doc["snr"] = status.lastPacketInfo.snr;
    doc["frequency_error"] = status.lastPacketInfo.frequencyerror;
    doc["unix_GS_time"] = now;
    JsonArray msgTTN = doc.createNestedArray("msgTTN");

    for (byte i=0 ; i<  (respLen-12);i++) {
      msgTTN.add(String(tmp[i], HEX));
    }
    serializeJson(doc, Serial);
    char buffer[256];
    serializeJson(doc, buffer);
    size_t n = serializeJson(doc, buffer);

    publish(buildTopic(topicMiniTTN).c_str(), buffer,n );
  }
  else {
    const size_t capacity = JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(11);
    DynamicJsonDocument doc(capacity);
    doc["station"] = configManager.getThingName();
    JsonArray station_location = doc.createNestedArray("station_location");
    station_location.add(configManager.getLatitude());
    station_location.add(configManager.getLongitude());
    doc["rssi"] = status.lastPacketInfo.rssi;
    doc["snr"] = status.lastPacketInfo.snr;
    doc["frequency_error"] = status.lastPacketInfo.frequencyerror;
    doc["unix_GS_time"] = now;
    doc["msg"] = String(tmp);

    serializeJson(doc, Serial);
    char buffer[256];
    serializeJson(doc, buffer);
    size_t n = serializeJson(doc, buffer);

    publish(buildTopic(topicMsg).c_str(), buffer,n );
  }
}

void  MQTT_Client::sendRawPacket(String packet) {
  ConfigManager& configManager = ConfigManager::getInstance();
  time_t now;
  time(&now);
  const size_t capacity = JSON_ARRAY_SIZE(2) + JSON_OBJECT_SIZE(10);
  DynamicJsonDocument doc(capacity);
  doc["station"] = configManager.getThingName();
  JsonArray station_location = doc.createNestedArray("station_location");
  station_location.add(configManager.getLatitude());
  station_location.add(configManager.getLongitude());
  doc["rssi"] = status.lastPacketInfo.rssi;
  doc["snr"] = status.lastPacketInfo.snr;
  doc["frequency_error"] = status.lastPacketInfo.frequencyerror;
  doc["unix_GS_time"] = now;
  doc["data"] = packet.c_str();
  serializeJson(doc, Serial);
  char buffer[256];
  serializeJson(doc, buffer);
  size_t n = serializeJson(doc, buffer);

  publish(buildTopic(topicRawPacket).c_str(), buffer,n );
}

void MQTT_Client::manageMQTTData(char *topic, uint8_t *payload, unsigned int length) {
  Radio& radio = Radio::getInstance();
  if (!strcmp(topic, "fossa/global/sat_pos_oled")) {
    manageSatPosOled((char*)payload, length);
  }

 // Remote_Reset
 if (!strcmp(topic, buildTopic((String(topicRemote) + String(topicRemoteReset)).c_str()).c_str())) {
    ESP.restart();
  }

// Remote_Ping
 if (!strcmp(topic, buildTopic((String(topicRemote) + String(topicRemotePing)).c_str()).c_str())) {
    radio.sendPing();
  }

// Remote_Frequency
 if (!strcmp(topic, buildTopic((String(topicRemote) + String(topicRemoteFreq)).c_str()).c_str())) {
    radio.remote_freq((char*)payload, length);
  }
// Remote_Bandwidth
 if (!strcmp(topic, buildTopic((String(topicRemote) + String(topicRemoteBw)).c_str()).c_str())) {
    radio.remote_bw((char*)payload, length);
  }
// Remote_spreading factor
if (!strcmp(topic, buildTopic((String(topicRemote) + String(topicRemoteSf)).c_str()).c_str())) {
    radio.remote_sf((char*)payload, length);
  }
// Remote_Coding rate
if (!strcmp(topic, buildTopic((String(topicRemote) + String(topicRemoteCr)).c_str()).c_str())) {
    radio.remote_cr((char*)payload, length);
  }
// Remote_Crc
if (!strcmp(topic, buildTopic((String(topicRemote) + String(topicRemoteCrc)).c_str()).c_str())) {
    radio.remote_crc((char*)payload, length);
  }
// Remote_Preamble Lenght
if (!strcmp(topic, buildTopic((String(topicRemote) + String(topicRemotePl)).c_str()).c_str())) {
    radio.remote_crc((char*)payload, length);
  }

// Remote_Begin_Lora
if (!strcmp(topic, buildTopic((String(topicRemote) + String(topicRemoteBl)).c_str()).c_str())) {
    radio.remote_begin_lora((char*)payload, length);
  }
// Remote_Begin_FSK
if (!strcmp(topic, buildTopic((String(topicRemote) + String(topicRemoteFs)).c_str()).c_str())) {
    radio.remote_begin_fsk((char*)payload, length);
  }




}

void MQTT_Client::manageSatPosOled(char* payload, size_t payload_len) {
  DynamicJsonDocument doc(60);
  char payloadStr[payload_len+1];
  memcpy(payloadStr, payload, payload_len);
  payloadStr[payload_len] = '\0';
  deserializeJson(doc, payload);
  status.satPos[0] = doc[0];
  status.satPos[1] = doc[1];
}

// Helper class to use as a callback
void manageMQTTDataCallback(char *topic, uint8_t *payload, unsigned int length) {
  ESP_LOGI (LOG_TAG,"Received MQTT message: %s : %.*s\n", topic, length, payload);
  MQTT_Client::getInstance().manageMQTTData(topic, payload, length);
}

void MQTT_Client::begin() {
  ConfigManager& configManager = ConfigManager::getInstance();
  setServer(configManager.getMqttServer(), configManager.getMqttPort());
  setCallback(manageMQTTDataCallback);
}