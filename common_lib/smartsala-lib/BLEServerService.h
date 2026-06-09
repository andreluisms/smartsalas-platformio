#ifndef BLEServerService_h
#define BLEServerService_h

#include <unordered_map>

#ifdef _WIN32
    #include <String> // Para Windows
    #include <Vector>
#else
    #include <string.h> // Para Linux
    #include <vector>
#endif

#include <Arduino.h>
// #include <BLEDevice.h>
// #include <BLEScan.h>
// #include <BLEAdvertisedDevice.h>
#include <NimBLEDevice.h>
#include <unordered_map>
#include "AwaitHttpService.h"
#include "Config.h"
#include "EnvironmentVariablesService.h"
#include "Structs.h"
#include "WiFiService.h"
#include "Hardware.h"

#define TIME_CONNECTION  5000 
#define TIME_WAITING_CONNECTION 30000

static BLEUUID CHARACTERISTIC_UUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static BLEUUID HARDWARE_UUID_CHARACTERISTIC_UUID("e6f1e7b0-5f6a-4c5d-9b46-31d2f7a6b9c1");
static BLEUUID SERVICE_UUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");

using namespace std;

class Hardware;

class BLEServerService 
{
  public:
    static int __countTypeSensor;
    static int __countTypeActuator;
    static vector<String> __sensors;
    static vector<struct HardwareRecord> __actuators;
    static BLEScan* __pBLEScan;
    static vector<BLEAdvertisedDevice*> __filteredDevices;
    static std::unordered_map<std::string, Hardware> __devicesMapped;
    static BLEDeviceConnect* __actuatorConnected;
    static bool __receivedData;
    
    BLEServerService();
    
    static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify); 
    BLEDeviceConnect* connectToDevice(BLEAdvertisedDevice* myDevice, bool validateConnection); 
    void initBLE(); 
    void stopScan(); 
    void scanDevices(); 
    void populateMap(); 
    void activeBLEScan(); 
    bool isSensor(String uuid);
    bool isAtuador(String uuid);
    bool connectMyDisp(BLEAdvertisedDevice* device); 
    int getCountDispsTypeSensor();
    void timer(); 
    bool connectToActuator(String uuidDevice);
    void disconnectToActuator();
    void sendMessageToActuator(String data);

    // getters and setters
    vector<String> getSensors();
    vector<struct HardwareRecord> getActuators();
    int getExpectedDevicesCount();
    int getMappedDevicesCount();
    bool hasMappedAllExpectedDevices();
    void addSensor(String uuid);
    void addActuator(HardwareRecord act);
    void newCicle();
    
    static void setCountTypeSensor(int count);
    static void setCounttypeActuator(int count);
    static int getCountTypeSensor();
    static int getCounttypeActuator();

    void closeConnections(vector<BLEDeviceConnect*> aux);

    bool isSensorListed(String uuid, int typeDisp);

    // metods task
    void continuousConnectionTask();
    static void startTaskBLEImpl(void*);
    void startTaskBLE();
  
};

#endif
