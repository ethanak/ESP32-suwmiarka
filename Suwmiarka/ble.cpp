#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "prefs.h"

BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;


#define SERVICE_UUID           "f3c8fc3c-cc3b-11ec-9d64-0242ac120002" // UART service UUID
#define CHARACTERISTIC_UUID_RX "f3c8fc3d-cc3b-11ec-9d64-0242ac120002"
#define CHARACTERISTIC_UUID_TX "f3c8fc3e-cc3b-11ec-9d64-0242ac120002"

//#define SERVICE_UUID pfs_uidstr[0]
//#define CHARACTERISTIC_UUID_RX pfs_uidstr[1]
//#define CHARACTERISTIC_UUID_TX  pfs_uidstr[2]


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
      std::string rxValue = pCharacteristic->getValue();
#if 0        
      if (rxValue.length() > 0) {
          printf("R: %s\n", rxValue.c_str());
      }
#endif
  };
};

void initBLE()
{
    //printf("SUID %s\nBTN %s\n", SERVICE_UUID, pfs_btname);
    BLEDevice::init(pfs_btname);

  // Create the BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
    BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
    pTxCharacteristic = pService->createCharacteristic(
										CHARACTERISTIC_UUID_TX,
										BLECharacteristic::PROPERTY_NOTIFY
									);
                      
    pTxCharacteristic->addDescriptor(new BLE2902());
    BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
											 CHARACTERISTIC_UUID_RX,
											BLECharacteristic::PROPERTY_WRITE
										);

    pRxCharacteristic->setCallbacks(new MyCallbacks());
    pService->start();
    pServer->getAdvertising()->start();
    
}

bool sayBLE(const char *c)
{
    if (!deviceConnected) return false;
    char *message=(char *)malloc(strlen(c)+3);
    strcpy(message,c);
    strcat(message,"\r\n");
    pTxCharacteristic->setValue(message);
    pTxCharacteristic->notify();
    free(message);
    return true;
}

void loopBLE()
{
    if (!pServer) return;
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        //Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        //printf("Connecting\n");
		// do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
}
