#include <ArduinoMqttClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"
#include <DHT.h>       //library DHT
#define DHTPIN D2      //pin DATA konek ke pin 2 Arduino
#define DHTTYPE DHT11  //tipe sensor DHT11

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;  // your network SSID (name)
char pass[] = SECRET_PASS;  // your network password (use for WPA, or use as key for WEP)

// To connect with SSL/TLS:
// 1) Change WiFiClient to WiFiSSLClient.
// 2) Change port value from 1883 to 8883.
// 3) Change broker value to a server with a known SSL/TLS root certificate
//    flashed in the WiFi module.

WiFiClientSecure wifiClient;
MqttClient mqttClient(wifiClient);
DHT dht(DHTPIN, DHTTYPE);  //set sensor + koneksi pin

const char broker[] = "02fa3ba5f83a4760bc66879c7e081d28.s1.eu.hivemq.cloud";
int port = 8883;
const char topic[] = "greenhouse:updated";  // Subscribe to all topics
const char* greenhouse_key = "2e3ae7b6-eee8-4b48-8871-c0c7c4659b15";
const int relayPin1 = D1;  // GPIO5
const int relayPin2 = D3;  // GPIO0
float humi, temp;          //deklarasi variabel
const long interval = 5000;
unsigned long previousMillis = 0;

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  wifiClient.setInsecure();
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  pinMode(relayPin1, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  digitalWrite(relayPin1, LOW);  // Initialize relay as off
  digitalWrite(relayPin2, LOW);  // Initialize relay as off
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }
  setup_wifi();

  // You can provide a unique client ID, if not set the library uses Arduino-millis()
  // Each client must have a unique client ID
  mqttClient.setId("clientId");

  // You can provide a username and password for authentication
  mqttClient.setUsernamePassword("everglo", "everglo2024");

  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(broker);

  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT connection failed! Error code = ");
    Serial.println(mqttClient.connectError());

    while (1)
      ;
  }

  Serial.println("You're connected to the MQTT broker!");
  Serial.println();

  Serial.print("Subscribing to all topics");
  Serial.println();

  // subscribe to all topics
  mqttClient.subscribe(topic);

  // set the message receive callback
  mqttClient.onMessage(onMqttMessage);

  Serial.print("Waiting for messages on all topics");
  Serial.println();
}


void loop() {
  // call poll() regularly to allow the library to receive MQTT messages and
  // send MQTT keep alives which avoids being disconnected by the broker
  mqttClient.poll();
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time a message was sent
    previousMillis = currentMillis;
    humi = dht.readHumidity();         //baca kelembaban
    temp = dht.readTemperature();      //baca suhu
    if (isnan(humi) || isnan(temp)) {  //jika tidak ada hasil
      Serial.println("DHT11 tidak terbaca... !");
      return;
    } else {  //jika ada hasilnya
      updateGreenhouse((temp < 0 ? temp/-1 : temp), humi);
      return;
    }
  }
}

void onMqttMessage(int messageSize) {
  // we received a message, print out the topic and contents
  Serial.print("Received a message with topic '");
  Serial.print(mqttClient.messageTopic());
  Serial.print("', length ");
  Serial.print(messageSize);
  Serial.println(" bytes:");

  // Create a buffer to hold the incoming message
  char message[messageSize + 1];
  int index = 0;

  // Read the message into the buffer
  while (mqttClient.available()) {
    message[index++] = (char)mqttClient.read();
  }
  message[messageSize] = '\0';  // Null-terminate the buffer

  // Print the received message
  Serial.println("Message:");
  Serial.println(message);

  // Parse JSON
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.f_str());
    return;
  }

  // Extract values
  const char* greenhouseId = doc["greenhouseId"];
  bool statusBlower = doc["statusBlower"];
  bool statusHeater = doc["statusHeater"];
  // Compare greenhouseId with greenhouse_key
  if (strcmp(greenhouseId, greenhouse_key) == 0) {

    //handle blower
    if (statusBlower) {
      digitalWrite(relayPin1, LOW);  // Turn relay 1 off
    } else {
      digitalWrite(relayPin1, HIGH);  // Turn relay 1 on
    }

    // handle heater
    if (statusHeater) {
      digitalWrite(relayPin2, LOW);  // Turn relay 2 off
    } else {
      digitalWrite(relayPin2, HIGH);  // Turn relay 2 on
    }
  }
}

void updateGreenhouse(float temperature, float humidity) {
  bool retained = false;
  int qos = 1;
  bool dup = false;
  String requestBody = "{\"ownedGreenhouse\":\"" + String(greenhouse_key) + "\",\"airTemperature\":" + String(temperature) + ",\"humidity\":" + String(humidity) + "}";
  Serial.println("Greenhouse data created : " + requestBody);
  mqttClient.beginMessage("greenhouseData:created", requestBody.length(), retained, qos, dup);
  mqttClient.print(requestBody);
  mqttClient.endMessage();
}
