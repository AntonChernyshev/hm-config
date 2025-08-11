/*
  ESP32 Firmware to simulate a Helium Gateway for configuration.

  Version 2: Adds Wi-Fi and MQTT capabilities.

  This sketch creates a Bluetooth Low Energy (BLE) server that mimics the
  services and characteristics of a real Helium gateway.

  When configuration data is received via BLE, it is published to an
  MQTT broker on the central core computer.
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <PubSubClient.h>

// --- Configuration ---
// Replace with your network credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Replace with your MQTT Broker's IP address
const char* mqtt_server = "YOUR_MQTT_BROKER_IP";
const int mqtt_port = 1883;

// --- BLE UUIDs ---
// Service UUID
#define HELIUM_SERVICE_UUID "0fda92b2-44a2-4af2-84f5-fa682baa2b8d"

// Characteristic UUIDs
#define ONBOARDING_KEY_CHARACTERISTIC_UUID "d083b2bd-be16-4600-b397-61512ca2f5ad"
#define PUBLIC_KEY_CHARACTERISTIC_UUID "0a852c59-50d3-4492-bfd3-22fe58a24f01"
#define WIFI_SERVICES_CHARACTERISTIC_UUID "d7515033-7e7b-45be-803f-c8737b171a29"
#define WIFI_CONFIGURED_SERVICES_CHARACTERISTIC_UUID "e125bda4-6fb8-11ea-bc55-0242ac130003"
#define WIFI_REMOVE_CHARACTERISTIC_UUID "8cc6e0b3-98c5-40cc-b1d8-692940e6994b"
#define DIAGNOSTICS_CHARACTERISTIC_UUID "b833d34f-d871-422c-bf9e-8e6ec117d57e"
#define MAC_ADDRESS_CHARACTERISTIC_UUID "9c4314f2-8a0c-45fd-a58d-d4a7e64c3a57"
#define LIGHTS_CHARACTERISTIC_UUID "180efdef-7579-4b4a-b2df-72733b7fa2fe"
#define WIFI_SSID_CHARACTERISTIC_UUID "7731de63-bc6a-4100-8ab1-89b2356b038b"
#define ASSERT_LOCATION_CHARACTERISTIC_UUID "d435f5de-01a4-4e7d-84ba-dfd347f60275"
#define ADD_GATEWAY_CHARACTERISTIC_UUID "df3b16ca-c985-4da2-a6d2-9b9b9abdb858"
#define WIFI_CONNECT_CHARACTERISTIC_UUID "398168aa-0111-4ec0-b1fa-171671270608"
#define ETHERNET_ONLINE_CHARACTERISTIC_UUID "e5866bd6-0288-4476-98ca-ef7da6b4d289"
#define SOFTWARE_VERSION_CHARACTERISTIC_UUID "c0b64050-697d-463a-a33f-70c4825731f8"

BLEServer* pServer = NULL;
WiFiClient espClient;
PubSubClient client(espClient);

// Forward declarations
void publish_ble_write(const char* characteristic_uuid, const char* value);

// BLE Callback handler for Write Events
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      const char* uuid = pCharacteristic->getUUID().toString().c_str();

      if (value.length() > 0) {
        Serial.print("Received Write on characteristic ");
        Serial.print(uuid);
        Serial.print(" with value: ");
        Serial.println(value.c_str());

        // Publish the received data to the MQTT broker
        publish_ble_write(uuid, value.c_str());
      }
    }
};

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect_mqtt() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void publish_ble_write(const char* characteristic_uuid, const char* value) {
  if (!client.connected()) {
    reconnect_mqtt();
  }

  // Construct a topic name, e.g., "helium/simulator/ble_write/d435f5de-..."
  char topic[128];
  snprintf(topic, sizeof(topic), "helium/simulator/ble_write/%s", characteristic_uuid);

  Serial.printf("Publishing to MQTT topic %s: %s\n", topic, value);
  client.publish(topic, value);
}


void setup() {
  Serial.begin(115200);

  // Setup WiFi and MQTT
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  Serial.println("Starting BLE Server for Helium Gateway Simulator");

  BLEDevice::init("Helium Gateway Simulator");
  pServer = BLEDevice::createServer();
  BLEService *pHeliumService = pServer->createService(HELIUM_SERVICE_UUID);

  // Create Characteristics (as before)
  pHeliumService->createCharacteristic(ONBOARDING_KEY_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ)->setValue("placeholder_onboarding_key");
  pHeliumService->createCharacteristic(PUBLIC_KEY_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ)->setValue("placeholder_public_key");
  pHeliumService->createCharacteristic(WIFI_SERVICES_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY)->addDescriptor(new BLE2902());
  pHeliumService->createCharacteristic(WIFI_CONFIGURED_SERVICES_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY)->addDescriptor(new BLE2902());
  pHeliumService->createCharacteristic(WIFI_REMOVE_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE)->setCallbacks(new MyCallbacks());
  pHeliumService->createCharacteristic(DIAGNOSTICS_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY)->addDescriptor(new BLE2902());
  pHeliumService->createCharacteristic(MAC_ADDRESS_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ)->setValue("DE:AD:BE:EF:FE:ED");
  pHeliumService->createCharacteristic(LIGHTS_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE)->setCallbacks(new MyCallbacks());
  pHeliumService->createCharacteristic(WIFI_SSID_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY)->addDescriptor(new BLE2902());
  pHeliumService->createCharacteristic(ASSERT_LOCATION_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE)->setCallbacks(new MyCallbacks());
  pHeliumService->createCharacteristic(ADD_GATEWAY_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE)->setCallbacks(new MyCallbacks());
  pHeliumService->createCharacteristic(WIFI_CONNECT_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE)->setCallbacks(new MyCallbacks());
  pHeliumService->createCharacteristic(ETHERNET_ONLINE_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY)->addDescriptor(new BLE2902());
  pHeliumService->createCharacteristic(SOFTWARE_VERSION_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ)->setValue("2024.01.01.SIM");

  pHeliumService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(HELIUM_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("Helium Gateway Simulator is advertising...");
}

void loop() {
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();
  // No delay needed here as the loop is non-blocking
}
