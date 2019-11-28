#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

#include "config.h"
#include "Outlet.h"


// LEDs and pins
#define PIN_DO_RGB_G 5
#define PIN_DO_RGB_B 4
#define PIN_DI_BUTTON 13
#define PIN_DO_RELAIS_1 12
#define PIN_DO_RELAIS_2 14

#define LEDON    LOW
#define LEDOFF   HIGH

#define RELAISOPEN    LOW
#define RELAISCLOSE   HIGH

// global vars
bool              PIN_RGB_ON = 0;


// objects
Ticker           ticker;
WiFiClient       espClient;
void mqttCallback(char* topic, byte* payload, unsigned int length);
PubSubClient mqttClient(espClient);

Outlet            outletLeft(PIN_DO_RELAIS_1);
Outlet            outletRight(PIN_DO_RELAIS_2);



///////////////////////////////////////////////////////////////////////////
//   LED Ticker
///////////////////////////////////////////////////////////////////////////

void tickGreen() {
  PIN_RGB_ON = !PIN_RGB_ON;
  digitalWrite(PIN_DO_RGB_G, PIN_RGB_ON);
}

void tickBlue() {
  PIN_RGB_ON = !PIN_RGB_ON;
  digitalWrite(PIN_DO_RGB_B, PIN_RGB_ON);
}

void tickerOff() {
  ticker.detach();
  digitalWrite(PIN_DO_RGB_G, LEDOFF);
  digitalWrite(PIN_DO_RGB_B, LEDOFF);
}


///////////////////////////////////////////////////////////////////////////
//   ArduinoOTA
///////////////////////////////////////////////////////////////////////////

void setupArduinoOTA() {
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.print("OTA error: ");
    Serial.println(error);
    switch (error) {
      case OTA_AUTH_ERROR:    Serial.println("OTA: Auth Failed"); break;
      case OTA_BEGIN_ERROR:   Serial.println("OTA: Begin Failed"); break;
      case OTA_CONNECT_ERROR: Serial.println("OTA: Connect Failed"); break;
      case OTA_RECEIVE_ERROR: Serial.println("OTA: Receive Failed"); break;
      case OTA_END_ERROR:     Serial.println("OTA: End Failed"); break;
    }

    ESP.restart();
  });

  ArduinoOTA.onStart([]() {
    ticker.attach(0.2, tickBlue);
  });
  ArduinoOTA.onEnd([]() {
    tickerOff();
  });

  ArduinoOTA.setPort(SETUP_OTA_PORT);
  ArduinoOTA.setPassword(SETUP_OTA_PASSWORD);
  ArduinoOTA.setHostname(SETUP_OTA_HOSTNAME);
  ArduinoOTA.begin();
}



///////////////////////////////////////////////////////////////////////////
//   MQTT Client
///////////////////////////////////////////////////////////////////////////
void mqttReconnect() {
  // Loop until we're reconnected
  if (!mqttClient.connected()) {

    // Attempt to connect
    if (mqttClient.connect(
      (String(SETUP_MQTT_CLIENTID) + "-" + ESP.getChipId()).c_str(),
      SETUP_MQTT_USER,
      SETUP_MQTT_PASSWORD,
      (String(SETUP_MQTT_PATH) + "/connection").c_str(),
      1,
      1,
      "offline"
    )) {
      mqttClient.subscribe((String(SETUP_MQTT_PATH) + "/+").c_str());

      mqttClient.publish(
        (String(SETUP_MQTT_PATH) + "/connection").c_str(),
        "online",
        true
      );

      mqttClient.publish(
        (String(SETUP_MQTT_PATH) + "/ip").c_str(),
        WiFi.localIP().toString().c_str(),
        true
      );

      mqttClient.publish(
        (String(SETUP_MQTT_PATH) + "/restartReason").c_str(),
        ESP.getResetReason().c_str(),
        true
      );

      mqttClient.publish(
        (String(SETUP_MQTT_PATH) + "/outletLeft_State").c_str(),
        outletLeft.getStateStr().c_str(),
        true
      );

      mqttClient.publish(
        (String(SETUP_MQTT_PATH) + "/outletRight_State").c_str(),
        outletRight.getStateStr().c_str(),
        true
      );
    }else{
      Serial.print("MQTT connection failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again later");

      // Wait a second before retrying
      delay(1000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String data = String("");
  for (unsigned int i = 0; i < length; i++) {
    data += (char) payload[i];
  }

  int prefixLength = String(SETUP_MQTT_PATH).length();
  String subtopic = String(topic).substring(prefixLength);

  // mqtt callback
  Serial.println("---");
  Serial.println("### MQTT Event");
  Serial.print("Sub-Topic: ");
  Serial.println(subtopic);
  Serial.print("Payload: ");
  Serial.println(data);
  Serial.print("Action: ");

  if(subtopic.equals("/outletLeft"))
  {
      outletLeft.setState(data);
      mqttClient.publish(
        (String(SETUP_MQTT_PATH) + "/outletLeft_State").c_str(),
        outletLeft.getStateStr().c_str(),
        true
      );
  }
  else if(subtopic.equals("/outletRight"))
  {
      outletRight.setState(data);
      mqttClient.publish(
        (String(SETUP_MQTT_PATH) + "/outletRight_State").c_str(),
        outletRight.getStateStr().c_str(),
        true
      );
  }
  else if(subtopic.equals("/reboot")) {
    Serial.println("reboot");
    ESP.restart();
  }
  else {
    Serial.println("-");
  }

  Serial.println("---");
}

void mqttLoop() {
  if(!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();
}

void setupMQTTClient() {
  mqttClient.setServer(SETUP_MQTT_BROKER, SETUP_MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
}


void setup()
{
  Serial.begin(9600);

  pinMode(PIN_DO_RGB_G, OUTPUT);
  pinMode(PIN_DO_RGB_B, OUTPUT);

  ticker.attach(0.1, tickBlue);

  Serial.print("ESP ");
  Serial.println(ESP.getChipId());
  Serial.println("--------------------");


  Serial.print("- Connecting to wifi");
  WiFi.begin(SETUP_WIFI_SSID, SETUP_WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  tickerOff();
  ticker.attach(0.1, tickGreen);

  Serial.print("- Connected, got IP address ");
  Serial.println(WiFi.localIP());

  Serial.println("- Setup OTA");
  setupArduinoOTA();

  Serial.println("- Setup MQTT client");
  setupMQTTClient();

  Serial.println("- Setup completed\n");
  tickerOff();
}

void loop()
{
  ArduinoOTA.handle();
  yield();

  mqttLoop();
  yield();
}
