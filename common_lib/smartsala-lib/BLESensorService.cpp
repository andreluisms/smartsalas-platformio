#include "BLESensorService.h"

static BLECharacteristic* pCharacteristicSensor;  
static BLECharacteristic* pCharacteristicHardwareUuid;
static bool deviceConnected;
static bool oldDeviceConnected = false;
static BLEServer* pServer;
EnvironmentVariablesService __environmentVariableService;
static DeviceType deviceType;
EquipmentService equipmentService;
static String equipmentState = "";
String Buffer = "";

void sendDataToServer(String data)
{
  if (deviceConnected)
  {
    if (SEND_DATA)
    {
      pCharacteristicSensor->setValue(std::string(data.c_str()));
      pCharacteristicSensor->notify();
      delay(100);
    }
  }
}

void initBLEClient(String deviceName, DeviceType devType)
{
  pinMode(LED, OUTPUT);
  deviceType = devType;

  Serial.println("[ATUADOR][BLE] Inicializando servidor BLE");
  Serial.println("[ATUADOR][BLE] Nome do dispositivo: " + deviceName);
  Serial.println("[ATUADOR][BLE] UUID servico: " + String(SERVICEUUID));
  Serial.println("[ATUADOR][BLE] UUID caracteristica: " + String(CHARACTERISTICUUID));

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

    pCharacteristicHardwareUuid = pService->createCharacteristic(
      HARDWARE_UUID_CHARACTERISTICUUID,
      NIMBLE_PROPERTY::READ);


  // pCharacteristicSensor->addDescriptor(new BLE2902());

  pCharacteristicSensor->setCallbacks(new MyCallbacks());

  String hardwareUuid = __environmentVariableService.getHardware().uuid;
  pCharacteristicHardwareUuid->setValue(std::string(hardwareUuid.c_str()));

  Serial.println("[BLESensorService>>initBLEClient]: Inicia servico");
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICEUUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();

  Serial.println("[BLESensorService>>initBLEClient]: Esperando conexao do cliente para notificar...");
  Serial.println("[ATUADOR][BLE] Advertising ativo");
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
  String hardwareUuid = __environmentVariableService.getHardware().uuid;
  pCharacteristicHardwareUuid->setValue(std::string(hardwareUuid.c_str()));
  pCharacteristicSensor->setValue(std::string(hardwareUuid.c_str()));
  //pCharacteristicSensor->notify();

  deviceConnected = true;
  SEND_DATA = false;
  Buffer = "";

  Serial.println("[MyServerCallbacks::onConnect()] Conectado");
  Serial.println("[ATUADOR][BLE] UUID de hardware exposto ao controlador: " + hardwareUuid);

  delay(100);
}

void MyServerCallbacks::onDisconnect(BLEServer *pServer)
{
  deviceConnected = false;
  SEND_DATA = false;
  digitalWrite(LED, LOW);
  Serial.println("[MyServerCallbacks::onDisconnect()] Desconectado");

  delay(100);
  BLEDevice::startAdvertising();
  Serial.println("[ATUADOR][BLE] Advertising reativado apos desconexao");
}

void MyCallbacks::onWrite(BLECharacteristic *pCharacteristic)
{

  std::string response = pCharacteristic->getValue();
  String mensagem = String(response.c_str());
  mensagem.trim();
  Serial.println("[MyCallbacks::onWrite()] Recebeu pacote: " + mensagem);

  if (mensagem == GET_DATA)
  {
    Serial.println("[ATUADOR][BLE] Handshake GET_DATA recebido");
    SEND_DATA = true;
  }
  else if (deviceType == ATUADOR)
  {
    Buffer += mensagem;

    if (mensagem.indexOf(END_DATA) != -1)
    {
      Serial.println("[MyCallbacks::onWrite()] ATUADOR - (ONWRITE) COMANDO COMPLETO PARA O EQUIPAMENTO");
      Buffer.replace(END_DATA, "");
      COMMAND = Buffer;
      Buffer = "";
      equipmentState = "";
      SEND_DATA = true;
    }
  }
}
