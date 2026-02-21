#include "BLESensorService.h"

static BLECharacteristic* pCharacteristicSensor;  
static bool deviceConnected;
static BLEServer* pServer;
EnvironmentVariablesService __environmentVariableService;
static DeviceType deviceType;
EquipmentService equipmentService;
static String equipmentState = "";
static String receivedData = "";

void sendDataToServer(String data)
{
  if (deviceConnected)
  {
    if (SEND_DATA)
    {
      pCharacteristicSensor->setValue(data.c_str());
      pCharacteristicSensor->notify();
      delay(100);
    }
  }
}

void initBLEClient(String deviceName, DeviceType devType)
{
  pinMode(LED, OUTPUT);
  deviceType = devType;

  Serial.println("========================================");
  Serial.println("[BLE_CLIENT]: Set Name Disp");
  BLEDevice::init(std::string(deviceName.c_str()));

  Serial.println("[BLE_CLIENT]: Create server");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  Serial.println("[BLE_CLIENT]: Create Service");
  BLEService *pService = pServer->createService(SERVICEUUID);

  Serial.println("[BLE_CLIENT]: Create Characteristic");
  // pCharacteristicSensor = pService->createCharacteristic(
  //     CHARACTERISTICUUID,
  //     BLECharacteristic::PROPERTY_READ |
  //         BLECharacteristic::PROPERTY_WRITE |
  //         BLECharacteristic::PROPERTY_NOTIFY |
  //         BLECharacteristic::PROPERTY_INDICATE);

  pCharacteristicSensor = pService->createCharacteristic(
      CHARACTERISTICUUID,
          NIMBLE_PROPERTY::READ |
          NIMBLE_PROPERTY::WRITE |
          NIMBLE_PROPERTY::NOTIFY |
          NIMBLE_PROPERTY::INDICATE);


  // pCharacteristicSensor->addDescriptor(new BLE2902());

  pCharacteristicSensor->setCallbacks(new MyCallbacks());

  Serial.println("[BLE_CLIENT]: Start Service");
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICEUUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();

  Serial.println("Waiting a client connection to notify...");
}

void MyServerCallbacks::onConnect(BLEServer *pServer)
{

  digitalWrite(LED, HIGH);
  pCharacteristicSensor->setValue(__environmentVariableService.getHardware().uuid.c_str());
  pCharacteristicSensor->notify();

  deviceConnected = true;
  SEND_DATA = false;

  Serial.println("===============================================");
  Serial.println("[BLESensorService] CONECTADO");

  delay(100);
}

void MyServerCallbacks::onDisconnect(BLEServer *pServer)
{
  deviceConnected = false;
  digitalWrite(LED, LOW);
  Serial.println("===============================================");
  Serial.println("[BLESensorService] DESCONECTADO");

  ESP.restart();
}

void MyCallbacks::onWrite(BLECharacteristic *pCharacteristic)
{

  std::string response = pCharacteristic->getValue();
  Serial.println("===============================================");
  Serial.println("[BLESensorService] Receive packet: " + String(response.c_str()));

  if (String(GET_DATA).equals(response.c_str()))
  {
    SEND_DATA = true;
  }
  else if (deviceType == ATUADOR)
  {
    if (String(END_DATA).equals(response.c_str()))
    {
      Serial.println("===============================================");
      Serial.println("[BLESensorService] ATUADOR - (ONWRITE) COMMANDO PARA O EQUIPAMENTO");
      SEND_DATA = true;
      COMMAND = receivedData;

      equipmentState = "";
      receivedData = "";
    }
    else
    {
      receivedData = receivedData + response.c_str();
    }
  }
}
