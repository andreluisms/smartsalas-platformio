#ifndef BLESensorService_h
#define BLESensorService_h

// #include <BLEDevice.h>
// #include <BLEServer.h>
// #include <BLEUtils.h>
// #include <BLE2902.h>
#include <NimBLEDevice.h>
#include <Arduino.h>

#ifdef _WIN32
#include <String> // Para Windows
#else
#include <string.h> // Para Linux
#endif

#include "EquipmentService.h"
#include "EnvironmentVariablesService.h"
#include "Enums.h"
#include "Structs.h"

#define CHARACTERISTICUUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define SERVICEUUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define GET_DATA "GET_DATA"
#define END_DATA "END_DATA"
#define LED 2

void sendDataToServer(String data);
void initBLEClient(String deviceName, DeviceType devType);
void handleBLEConnectionState();

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer);

  void onDisconnect(BLEServer *pServer);
};

class MyCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic);
};

#endif
