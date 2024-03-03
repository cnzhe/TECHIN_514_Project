#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define ONE_WIRE_BUS 5

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long previousMillis = 0;
const long interval = 1000;

#define SERVICE_UUID        "ea8dadaf-21bd-4f27-8f01-1cbb4cc42a92"
#define CHARACTERISTIC_UUID "4c41c3f7-027f-4004-b1f1-00ee10411826"

float temperatureSum = 0.0;
int readingsCount = 0;

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

// DSP Algorithm to calculate real-time average temperature
float calculateAverageTemperature(float newTemperature) {
    temperatureSum += newTemperature;
    readingsCount++;
    return temperatureSum / readingsCount;
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE work!");

    BLEDevice::init("514Sensing");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setValue("Hello World");
    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Characteristic defined! Now you can read it on your phone!");
  
    // Initialize DS18B20 sensor
    sensors.begin();
}

void loop() {
    if (deviceConnected) {
        // Read temperature from DS18B20 sensor
        sensors.requestTemperatures();
        float temperatureCelsius = sensors.getTempCByIndex(0);

        // Calculate real-time average temperature using DSP algorithm
        float averageTemperature = calculateAverageTemperature(temperatureCelsius);

        // Convert temperatures to strings and update BLE characteristic
        char tempString[15];  // Buffer to store current and average temperatures as a string
        snprintf(tempString, sizeof(tempString), "%.2f, %.2f", temperatureCelsius, averageTemperature);
        pCharacteristic->setValue(tempString);
        pCharacteristic->notify();
        
        Serial.print("Notify values: ");
        Serial.println(tempString);
    }

    // Disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);  // Give the Bluetooth stack the chance to get things ready
        pServer->startAdvertising();  // Advertise again
        Serial.println("Start advertising");
        oldDeviceConnected = deviceConnected;
    }

    // Connecting
    if (deviceConnected && !oldDeviceConnected) {
        // Do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }

    delay(1000);
}
