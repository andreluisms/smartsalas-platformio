#include "BLESensorService.h"

static BLECharacteristic* pCharacteristicSensor;  
static bool deviceConnected;
static bool oldDeviceConnected = false;
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

  Serial.println("[BLESensorService>>initBLEClient]: Muda nome do dispositivo");
  BLEDevice::init(std::string(deviceName.c_str()));

  Serial.println("[BLESensorService>>initBLEClient]: cria servidor");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  Serial.println("[BLESensorService>>initBLEClient]: Cria servico");
  BLEService *pService = pServer->createService(SERVICEUUID);

  Serial.println("[BLESensorService>>initBLEClient]: Cria caracteristica");
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

  Serial.println("[BLESensorService>>initBLEClient]: Inicia servico");
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICEUUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();

  Serial.println("[BLESensorService>>initBLEClient]: Esperando conexao do cliente para notificar...");
}

void handleBLEConnectionState()
{
  if (!deviceConnected && oldDeviceConnected)
  {
    delay(500);
    pServer->startAdvertising();
    Serial.println("Bluetooth anunciando novamente...");
    oldDeviceConnected = deviceConnected;
  }

  if (deviceConnected && !oldDeviceConnected)
  {
    oldDeviceConnected = deviceConnected;
  }
}

void MyServerCallbacks::onConnect(BLEServer *pServer)
{
  digitalWrite(LED, HIGH);
  pCharacteristicSensor->setValue(__environmentVariableService.getHardware().uuid.c_str());
  // pCharacteristicSensor->notify();

  deviceConnected = true;
  SEND_DATA = false;

  Serial.println("[MyServerCallbacks::onConnect()] Conectado");

  delay(100);
}

void MyServerCallbacks::onDisconnect(BLEServer *pServer)
{
  deviceConnected = false;
  Serial.println("[MyServerCallbacks::onDisconnect()] Master Desconectou!");
}

void MyCallbacks::onWrite(BLECharacteristic *pCharacteristic)
{

  std::string response = pCharacteristic->getValue();
  String mensagem = String(response.c_str());
  mensagem.trim();
  Serial.println("[MyCallbacks::onWrite()] Recebeu pacote: " + mensagem);

  if (mensagem == GET_DATA)
  {
    Serial.println("[Aviso] Master solicitou dados. Retornando status dos sensores...");
    if (deviceType == SENSOR)
      SEND_DATA = true;
  }
  else if (deviceType == ATUADOR)
  {
    if (mensagem == END_DATA)
    {
      Serial.println("[MyCallbacks::onWrite()] ATUADOR - (ONWRITE) COMMANDO PARA O EQUIPAMENTO");
      SEND_DATA = true;
      COMMAND = receivedData;

      equipmentState = "";
      receivedData = "";
    }
    else
    {
      receivedData = receivedData + mensagem;
    }
  }
}
