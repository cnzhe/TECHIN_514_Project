#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <AccelStepper.h>


#define MOTOR_PIN_1 D0
#define MOTOR_PIN_2 D1
#define MOTOR_PIN_3 D2
#define MOTOR_PIN_4 D3
#define STEPS_PER_REVOLUTION 1100
#define MID_POINT STEPS_PER_REVOLUTION / 2

Adafruit_SSD1306 display(128, 64, &Wire, -1);
AccelStepper stepper(AccelStepper::FULL4WIRE, MOTOR_PIN_1, MOTOR_PIN_2, MOTOR_PIN_3, MOTOR_PIN_4);

static BLEUUID serviceUUID("ea8dadaf-21bd-4f27-8f01-1cbb4cc42a92");
static BLEUUID charUUID("4c41c3f7-027f-4004-b1f1-00ee10411826");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

float temperature = 0.0;

void updateDisplayAndMotor(float currentTemp, float avgTemp) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Temperature: ");
    display.print(currentTemp, 2);
    display.print(" C");

    display.setCursor(0, 10);
    display.print("Average: ");
    display.print(avgTemp, 2);
    display.print(" C");

    display.display();

    // Move motor based on currentTemp
    if (currentTemp > 30) {
        stepper.moveTo(MID_POINT - (STEPS_PER_REVOLUTION / 20)); // around 30 degrees to the right (HOT label)
    } else {
        stepper.moveTo(MID_POINT + (STEPS_PER_REVOLUTION / 20)); // around 30 degrees to the left (COLD label)
    }
    while (stepper.distanceToGo() != 0) {
        stepper.run();
    }
}

void notifyCallback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify) {
    // Extract current and average temperatures from the received data
    float currentTemperature = 0.0;
    float avgTemperature = 0.0;
    if (sscanf((char*)pData, "%f, %f", &currentTemperature, &avgTemperature) == 2) {
        updateDisplayAndMotor(currentTemperature, avgTemperature);
    } else {
        Serial.println("Invalid data format");
    }
}

class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
    }

    void onDisconnect(BLEClient* pclient) {
        connected = false;
        Serial.println("onDisconnect");
    }
};

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        Serial.print("BLE Advertised Device found: ");
        Serial.println(advertisedDevice.toString().c_str());

        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            doScan = true;
        }
    }
};

bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());

    BLEClient* pClient = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    pClient->connect(myDevice);
    Serial.println(" - Connected to server");
    pClient->setMTU(517);

    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
        Serial.print("Failed to find our service UUID: ");
        Serial.println(serviceUUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    Serial.println(" - Found our service");

    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
        Serial.print("Failed to find our characteristic UUID: ");
        Serial.println(charUUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    Serial.println(" - Found our characteristic");

    if (pRemoteCharacteristic->canRead()) {
        std::string value = pRemoteCharacteristic->readValue();
        Serial.print("The characteristic value was: ");
        Serial.println(value.c_str());
    }

    if (pRemoteCharacteristic->canNotify())
        pRemoteCharacteristic->registerForNotify(notifyCallback);

    connected = true;
    return true;
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting Arduino BLE Client application...");
    BLEDevice::init("");

    stepper.setMaxSpeed(1000);
    stepper.setAcceleration(500);
    stepper.moveTo(MID_POINT); // Initialize to middle position
    while (stepper.distanceToGo() != 0) {
        stepper.run();
    }

    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Initializing...");
    display.display();

    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);
}

void loop() {
    if (doConnect == true) {
        if (connectToServer()) {
            Serial.println("We are now connected to the BLE Server.");
        } else {
            Serial.println("We have failed to connect to the server; there is nothing more we will do.");
        }
        doConnect = false;
    }

    if (connected) {
        String newValue = "Time since boot: " + String(millis() / 1000);
        pRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());
    } else if (doScan) {
        BLEDevice::getScan()->start(0);
    }

    delay(1000);
}
