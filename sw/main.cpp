#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include "BasicStepperDriver.h"


// LEDs and pins
#define PIN_D_RGB_G 12
#define PIN_D_RGB_B 13
#define PIN_D_RGB_R 15

#define LEDON    LOW
#define LEDOFF   HIGH


// WiFi
#define SETUP_WIFI_SSID "MY_SSID"
#define SETUP_WIFI_PASSWORD "**********"


// MQTT
#define SETUP_MQTT_BROKER "**********"
#define SETUP_MQTT_PORT 1883
#define SETUP_MQTT_PATH "/home/wifiplug"
#define SETUP_MQTT_USER "wifiplug"
#define SETUP_MQTT_PASSWORD "**********"
#define SETUP_MQTT_CLIENTID "WIFIPLUG"


// OTA Updater
#define SETUP_OTA_HOSTNAME "wifiplug"
#define SETUP_OTA_PASSWORD "**********"
#define SETUP_OTA_PORT 8266

// Bot specific
#define MOTOR_STEPS 200
#define MOTOR_SPEED_OUT 180
#define MOTOR_SPEED_IN 50
#define MOTOR_CURVE 150
#define DIR 16
#define STEP 14
#define EN 5

BasicStepperDriver stepper(MOTOR_STEPS, DIR, STEP, EN);


// global vars
bool              PIN_RGB_ON = 0;
bool              MOTOR_POWERED = 0;
int               MOTOR_POWEROFF_COUNTER = 0;

// 0=initialize, 1=wait for calibration, 2=closing, 3=closed, 4=opening. 5=opened
int               state = 0;
int               sentState = 0;
String            states[] = {"initialize", "wait for calibration", "closing", "closed", "opening", "opened"};

// 0=closed 1=opened
bool              should = 0;

// objects
Ticker           ticker;
WiFiClient       espClient;
void mqttCallback(char* topic, byte* payload, unsigned int length);
PubSubClient mqttClient(espClient);


void motorRotate(int degree);



///////////////////////////////////////////////////////////////////////////
//   LED Ticker
///////////////////////////////////////////////////////////////////////////
void tickRed() {
  PIN_RGB_ON = !PIN_RGB_ON;
  digitalWrite(PIN_D_RGB_R, PIN_RGB_ON);
}
void tickGreen() {
  PIN_RGB_ON = !PIN_RGB_ON;
  digitalWrite(PIN_D_RGB_G, PIN_RGB_ON);
}
void tickBlue() {
  PIN_RGB_ON = !PIN_RGB_ON;
  digitalWrite(PIN_D_RGB_B, PIN_RGB_ON);
}
void tickYellow() {
  PIN_RGB_ON = !PIN_RGB_ON;
  digitalWrite(PIN_D_RGB_R, PIN_RGB_ON);
  digitalWrite(PIN_D_RGB_G, PIN_RGB_ON);
}
void tickerOff() {
  ticker.detach();
  digitalWrite(PIN_D_RGB_R, LOW);
  digitalWrite(PIN_D_RGB_G, LOW);
  digitalWrite(PIN_D_RGB_B, LOW);
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
    ticker.attach(0.2, tickYellow);
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
  for (int i = 0; i < length; i++) {
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

  if(subtopic.equals("/rotate")) {
    Serial.println("rotate");

    int degree = data.toInt();
    if(degree < 0 || degree > 0) {
      Serial.print("Degree: ");
      Serial.println(degree);

      motorRotate(degree);
    }
  }
  else if(subtopic.equals("/stop")) {
    Serial.println("stop");
    stepper.stop();
    stepper.disable();
    MOTOR_POWERED = 0;
  }
  else if(subtopic.equals("/reboot")) {
    Serial.println("reboot");
    ESP.restart();
  }
  else if(subtopic.equals("/open")) {
    Serial.println("open");
    should = 1;
  }
  else if(subtopic.equals("/close")) {
    Serial.println("close");
    should = 0;
  }
  else if(subtopic.equals("/state") && !MOTOR_POWERED) {
    Serial.println("set state for calibration");

    if(data.equals("closed")) {
      state = 3;
      should = 0;
    }
    else if(data.equals("opened")) {
      state = 5;
      should = 1;
    }
    else {
      state = 1;
      should = 0;
    }
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



///////////////////////////////////////////////////////////////////////////
//   MOTOR
///////////////////////////////////////////////////////////////////////////
void motorLoop () {
  unsigned wait_time_micros = stepper.nextAction();
  if(wait_time_micros > 0) {
    MOTOR_POWEROFF_COUNTER = 0;
    return;
  }

  if(should == 1 && state == 3) {
    state = 4;
    motorRotate(9990);
  }
  else if(should == 0 && state == 5) {
    state = 2;
    motorRotate(-9990);
  }
  else if(state == 2) {
    state = 3;
  }
  else if(state == 4) {
    state = 5;
  }

  if(state != sentState && state != 0) {
    mqttClient.publish(
      (String(SETUP_MQTT_PATH) + "/state").c_str(),
      states[state].c_str(),
      true
    );

    sentState = state;
  }

  if(MOTOR_POWERED && (state == 3 || state == 5)) {
    MOTOR_POWEROFF_COUNTER++;
  }
  if(MOTOR_POWEROFF_COUNTER >= 250 || state == 0 || state == 1) {
    stepper.disable();
    MOTOR_POWERED = 0;
  }
}

void motorRotate (int degree) {
  if(!MOTOR_POWERED) {
    stepper.enable();
    MOTOR_POWERED = 1;
  }

  if(degree > 0) {
    stepper.setRPM(MOTOR_SPEED_OUT);
  } else {
    stepper.setRPM(MOTOR_SPEED_IN);
  }

  MOTOR_POWEROFF_COUNTER = 0;
  stepper.startRotate(degree);
}




void setup()
{
  Serial.begin(9600);
  ticker.attach(0.1, tickBlue);

  Serial.print("ESP ");
  Serial.println(ESP.getChipId());
  Serial.println("--------------------");

  pinMode(PIN_D_RGB_R, OUTPUT);
  pinMode(PIN_D_RGB_G, OUTPUT);
  pinMode(PIN_D_RGB_B, OUTPUT);

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

  Serial.println("- Setup stepper motor library");
  stepper.begin(MOTOR_SPEED_OUT, 16);
  stepper.setSpeedProfile(stepper.LINEAR_SPEED, MOTOR_CURVE, MOTOR_CURVE);
  stepper.disable();

  Serial.println("- Setup completed\n");
  tickerOff();
}

void loop()
{
  ArduinoOTA.handle();
  yield();

  mqttLoop();
  yield();

  motorLoop();
  yield();
}
