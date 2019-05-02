#include <DHTesp.h>
#include "esp_system.h"

const int wdtTimeout = 60000;  //time in ms to trigger the watchdog
hw_timer_t *timer = NULL;

void IRAM_ATTR resetModule() {
  ets_printf("reboot\n");
  esp_restart();
}

DHTesp dht;

// DHT Sensor
const int DHTPin = 22;
// Soil moisture pin
const int MoisturePin = 32;

const int PumpPin = 14;
TempAndHumidity newValues;
int moisture;
/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID2 "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define CHARACTERISTIC_UUID3 "beb5483e-36e1-4688-b7f5-ea07361b26aa"
#define CHARACTERISTIC_UUID4 "beb5483e-36e1-4688-b7f5-ea07361b26ab"

uint32_t value = 0;
unsigned long afterwater_timer = 0;

bool deviceConnected = false;
bool oldDeviceConnected = false;
BLEServer* pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
BLECharacteristic *pCharacteristic2 = NULL;
BLECharacteristic *pCharacteristic3 = NULL;
BLECharacteristic *pCharacteristic4 = NULL;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();

      if (value.length() > 0) {
        Serial.println("*********");
        Serial.print("New value: ");
        for (int i = 0; i < value.length(); i++)
          Serial.print(value[i]);

        Serial.println();
        Serial.println("*********");
      }
    }
};

class MyCallbacksC4: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      if (value.length() == 1) {
        Serial.print("Watering for ");
        Serial.print(value[0]);
        Serial.println(" seconds.");
        digitalWrite(PumpPin, HIGH); // sets the digital pin 14 on
        delay(1000*value[0]);
        digitalWrite(PumpPin, LOW); // sets the digital pin 14 off
        afterwater_timer = 9;

      } else
      if (value.length() > 0) {
        Serial.println("*********");
        Serial.print("New value: ");
        for (int i = 0; i < value.length(); i++)
          Serial.print(value[i]);
        Serial.println();
        Serial.println("*********");
      }
    }
};
void setup() {
    // initialize the DHT sensor
  dht.setup(DHTPin, DHTesp::DHT11);

  afterwater_timer = 9;
  
  pinMode(PumpPin, OUTPUT);
  digitalWrite(PumpPin, LOW); // sets the digital pin 14 off


  Serial.begin(115200);
  Serial.println("Starting BLE work!");

  BLEDevice::init("PlantControllerV1");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ   |
                                         BLECharacteristic::PROPERTY_WRITE  |
                                         BLECharacteristic::PROPERTY_NOTIFY |
                                         BLECharacteristic::PROPERTY_INDICATE
                                       );

  pCharacteristic->setValue("Temperature");
  pCharacteristic->setCallbacks(new MyCallbacks());

  pCharacteristic->addDescriptor(new BLE2902());

// 2
  pCharacteristic2 = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID2,
                                         BLECharacteristic::PROPERTY_READ   |
                                         BLECharacteristic::PROPERTY_WRITE  |
                                         BLECharacteristic::PROPERTY_NOTIFY |
                                         BLECharacteristic::PROPERTY_INDICATE
                                       );

  pCharacteristic2->setValue("Humidity");
//  pCharacteristic2->setCallbacks(new MyCallbacks());

  pCharacteristic2->addDescriptor(new BLE2902());
// 3
  pCharacteristic3 = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID3,
                                         BLECharacteristic::PROPERTY_READ   |
                                         BLECharacteristic::PROPERTY_WRITE  |
                                         BLECharacteristic::PROPERTY_NOTIFY |
                                         BLECharacteristic::PROPERTY_INDICATE
                                       );

  pCharacteristic3->setValue("Moisture");
  pCharacteristic3->setCallbacks(new MyCallbacks());

  pCharacteristic3->addDescriptor(new BLE2902());

// 4
  pCharacteristic4 = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID4,
                                         BLECharacteristic::PROPERTY_READ   |
                                         BLECharacteristic::PROPERTY_WRITE  |
                                         BLECharacteristic::PROPERTY_NOTIFY |
                                         BLECharacteristic::PROPERTY_INDICATE
                                       );

  pCharacteristic4->setValue("Water!");
  pCharacteristic4->setCallbacks(new MyCallbacksC4());

  pCharacteristic4->addDescriptor(new BLE2902());


// end
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");

  // Watchdog timer
  timer = timerBegin(0, 80, true);                  //timer 0, div 80
  timerAttachInterrupt(timer, &resetModule, true);  //attach callback
  timerAlarmWrite(timer, wdtTimeout * 1000, false); //set time in us
  timerAlarmEnable(timer);                          //enable interrupt

}

void loop() {
  char strbuftemp[16];
  char strbufhum[16];
  char strbufmoist[16];

  timerWrite(timer, 0); //reset timer (feed watchdog)
  
   TempAndHumidity newValues = dht.getTempAndHumidity();
  while (dht.getStatus() != 0) {
    Serial.println("DHT11 error status: " + String(dht.getStatusString()));
    delay(100);
  }
  moisture = analogRead(MoisturePin);
  Serial.print("Humidity: ");
  Serial.print(newValues.humidity);
  Serial.print(" %\t Temperature: ");
  Serial.print(newValues.temperature);
  Serial.print(" *C ");            
  Serial.print("Analog: ");
  Serial.print(moisture);
  Serial.println(", afterwatertimer: ");
  Serial.print(afterwater_timer);
  Serial.println("");

  if (afterwater_timer > 0) {
    afterwater_timer--;
  } else {
    if (moisture > 2100) {
        Serial.println("Watering for 90 seconds.");
        digitalWrite(PumpPin, HIGH); // sets the digital pin 14 on
        delay(90000);
        digitalWrite(PumpPin, LOW); // sets the digital pin 14 off
        afterwater_timer = 40;
      }
  }
  
  
  //BlueTooth stuff
    // notify changed value
    if (deviceConnected) {
        sprintf(strbuftemp, "%2.1f C", newValues.temperature);
        pCharacteristic->setValue(strbuftemp);
        pCharacteristic->notify();
        delay(3); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
        sprintf(strbufhum, "%2.1f %%", newValues.humidity);
        pCharacteristic2->setValue(strbufhum);
        pCharacteristic2->notify();
        delay(3); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
        sprintf(strbufmoist, "%d Soil", moisture);
        pCharacteristic3->setValue(strbufmoist);
        pCharacteristic3->notify();
        value++;
        delay(3); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
    }
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
    // End BlueTooth
    delay(20000);
}
