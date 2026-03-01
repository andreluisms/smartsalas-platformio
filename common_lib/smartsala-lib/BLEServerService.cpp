#include "BLEServerService.h"

int BLEServerService::__countTypeSensor = 0;
int BLEServerService::__countTypeActuator = 0;
std::vector<String> BLEServerService::__sensors;
std::vector<struct HardwareRecord> BLEServerService::__actuators;
BLEScan *BLEServerService::__pBLEScan;
vector<BLEAdvertisedDevice *> BLEServerService::__filteredDevices;
unordered_map<string, Hardware> BLEServerService::__devicesMapped;
BLEDeviceConnect *BLEServerService::__actuatorConnected;
EnvironmentVariablesService __environmentVariables;
AwaitHttpService __clientAwaitHttpService;
Config __configuration;
WiFiService __wfService;

bool IN_CLASS = false;

bool HTTP_RECEIVED_DATA = false;
String HTTP_MESSAGE = "";

bool ENV_RECEIVED_DATA = false;
String ENV_MESSAGE = "";

bool HTTP_REQUEST = false;
bool ENV_REQUEST = false;

BLEServerService::BLEServerService()
{
  __countTypeSensor = 0;
  __countTypeActuator = 0;
}

void BLEServerService::notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  if (isNotify)
  {
    Serial.print("[BLEServerService::notifyCallback()] Callback pela caracteristica: ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" de tamanho " + length);
    Serial.println();
    Serial.print("[BLEServerService::notifyCallback()] Dado: ");
    String data = String(((char *)pData));
    Serial.println(data.substring(0, length));

    if (HTTP_REQUEST)
    {
      HTTP_RECEIVED_DATA = true;
      HTTP_MESSAGE = data.substring(0, length);
    }
    else
    {
      ENV_RECEIVED_DATA = true;
      ENV_MESSAGE = data.substring(0, length);
    }
  }
}

BLEDeviceConnect *BLEServerService::connectToDevice(BLEAdvertisedDevice *myDevice, bool validateConnection)
{
  BLEDeviceConnect *device = new BLEDeviceConnect();

  device->deviceFound = true;

  Serial.println("[BLEServerService::connectToDevice()]: Criar cliente");
  device->pClient = BLEDevice::createClient();

  bool ok = device->pClient->connect(myDevice);

  if (!ok)
  {
    device->deviceFound = false;
    return device;
  }

  delay(200);

  Serial.println("[BLEServerService::connectToDevice()]: Conectado ao disp");
  device->pRemoteService = device->pClient->getService(SERVICE_UUID);

  if (device->pRemoteService == nullptr)
  {
    Serial.print("[BLEServerService::connectToDevice()]: Falhou em encontrar o servico de UUID: ");
    Serial.println(SERVICE_UUID.toString().c_str());

    device->pClient->disconnect();
    device->deviceFound = false;

    return device;
  }

  delay(200);

  Serial.println("[BLEServerService::connectToDevice()]: Servico encontrado");
  device->pRemoteCharacteristic = device->pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);

  if (device->pRemoteCharacteristic == nullptr)
  {
    Serial.print("[BLEServerService::connectToDevice()]: Falhou em encontrar a caracteristica UUID: ");
    Serial.println(CHARACTERISTIC_UUID.toString().c_str());

    device->pClient->disconnect();
    device->deviceFound = false;

    return device;
  }

  Serial.println("[BLEServerService::connectToDevice()]: Encontrou nossa caracteristica");

  if (validateConnection)
  {
    Serial.println("[BLEServerService::connectToDevice()]: Conexao valida");
    delay(500);
    if (device->pRemoteCharacteristic->canRead())
    {
      Serial.println("[BLEServerService::connectToDevice()]: Caracteristica lida");
      delay(500);
      std::string value = device->pRemoteCharacteristic->readValue();

      Serial.print("[BLEServerService::connectToDevice()]: uuid pesquisado: ");
      Serial.println(value.c_str());

      if (!isAtuador(value.c_str()) && !isSensor(value.c_str()))
      {
        Serial.println("[BLEServerService::connectToDevice()]: dispositivo nao encontrado ");
        device->pClient->disconnect();
        device->deviceFound = false;

        return device;
      }

      device->uuid = value.c_str();
      Serial.print("[BLEServerService::connectToDevice()]: uuid the device: ");
      Serial.println(device->uuid.c_str());
    }
    else
    {
      Serial.println("[BLEServerService::connectToDevice()]: dispositivo nao encontrado ");
      device->pClient->disconnect();
      device->deviceFound = false;

      return device;
    }
  }

  if (device->pRemoteCharacteristic->canNotify())
    device->pRemoteCharacteristic->subscribe(true, notifyCallback);
  // device->pRemoteCharacteristic->registerForNotify(notifyCallback);

  Serial.println("[BLEServerService::connectToDevice()]: dispositivo conectado ");
  Serial.print("[BLEServerService::connectToDevice()]: ");
  Serial.println(CHARACTERISTIC_UUID.toString().c_str());
  Serial.print("[BLEServerService::connectToDevice()]: ");
  Serial.println(myDevice->getAddress().toString().c_str());

  Serial.print("[BLEServerService::connectToDevice()]: connId ");
  Serial.println(device->pClient->getConnId());
  Serial.print("[BLEServerService::connectToDevice()]: RSSI");
  Serial.println(device->pClient->getRssi());

  return device;
}

void BLEServerService::initBLE()
{
  Serial.println("[BLEServerService::initBLE()] Iniciando configuracoes BLE");
  BLEDevice::init("ESP32_CONTROLLER");
  Serial.println("[BLEServerService::initBLE()] Inicializa dispositivo");
}

void BLEServerService::activeBLEScan()
{
  __pBLEScan = BLEDevice::getScan();
  Serial.println("[BLEServerService::activeBLEScan()] novo Scan");
  __pBLEScan->setInterval(1349);
  Serial.println("[BLEServerService::activeBLEScan()] Set interval");
  __pBLEScan->setWindow(449);
  Serial.println("[BLEServerService::activeBLEScan()] Set window");
  __pBLEScan->setActiveScan(true);
  Serial.println("[BLEServerService::activeBLEScan()] Scan Ativo");
}

void BLEServerService::scanDevices()
{
  BLEScanResults foundDevices = __pBLEScan->start(5, false);

  for (int i = 0; i < foundDevices.getCount(); i++)
    __filteredDevices.push_back(new BLEAdvertisedDevice(foundDevices.getDevice(i)));

  // BLEAdvertisedDevice* disp = NULL;
  for (auto item : __filteredDevices)
  {
    BLEAdvertisedDevice *disp = item;
    Serial.print("[BLEServerService::scanDevices()]: ");
    Serial.println(disp->toString().c_str());
  }
}

void BLEServerService::stopScan()
{
  __pBLEScan->setActiveScan(false);
  __pBLEScan->stop();
}

/*
     Após fazer a busca, ele faz uma filtragem de quais dispositivos devem ser
*/
void BLEServerService::populateMap()
{
  for (auto disp : __filteredDevices)
  {
    if (__configuration.isDebug())
    {
      Serial.print("[BLEServerService::populateMap()]: Have UUID: ");
      Serial.println(disp->haveServiceUUID());
      Serial.print("[BLEServerService::populateMap()]: Dispositivo: ");
      Serial.println(disp->toString().c_str());
    }

    if (disp->haveServiceUUID() && disp->isAdvertisingService(SERVICE_UUID))
    {
      // bool deviceConnected = false;
      int index = 0, MAX = 4;

      do
      {
        if (__configuration.isDebug())
        {
          Serial.print("[BLEServerService::populateMap()]: Attempt Device: ");
          Serial.println(disp->toString().c_str());
          Serial.print("[BLEServerService::populateMap()]: Attempt Number: ");
          Serial.println(index);
        }

        if (connectMyDisp(disp))
          break;

        index++;
      } while (index < MAX);

      Serial.println("[BLEServerService::populateMap()]: Device Found");
    }
    else
    {
      Serial.println("[BLEServerService::populateMap()]: Device no listed");
    }
  }

  if (__devicesMapped.size() == 0)
    Serial.println("[BLEServerService::populateMap()]: no devices found");
}

bool BLEServerService::isSensor(String uuid)
{
  if (std::count(__sensors.begin(), __sensors.end(), uuid))
    return true;

  return false;
}

bool BLEServerService::isAtuador(String uuid)
{
  for (auto item : __actuators)
  {
    if (item.uuid.equals(uuid))
      return true;
  }

  return false;
}

bool BLEServerService::isSensorListed(String uuid, int typeDisp)
{
  Hardware disp;

  for (auto item : __devicesMapped)
  {
    disp = item.second;

    Serial.println("[BLEServerService::isSensorListed()]: mapping uuid: " + disp.getUuid());

    if (uuid.equals(disp.getUuid().c_str()) && disp.getTypeDisp() == typeDisp)
    {
      Serial.println("[BLEServerService::isSensorListed()]: device found");
      return true;
    }
  }

  return false;
}

bool BLEServerService::connectMyDisp(BLEAdvertisedDevice *device)
{
  Serial.println();
  Serial.println("[BLEServerService::connectMyDisp()]: connecting to my devices...");

  BLEDeviceConnect *deviceConnected = connectToDevice(device, true);

  if (deviceConnected->deviceFound)
  {
    Serial.println("[BLEServerService::connectMyDisp()]: forming successfull a connection");

    Hardware disp;
    disp.setBLEAdvertisedDevice(device);
    disp.setUuid(deviceConnected->uuid.c_str());

    if (isSensor(deviceConnected->uuid.c_str()))
    {
      Serial.println("[BLEServerService::connectMyDisp()]: Is Sensor ");
      disp.setTypeDisp(TYPE_SENSOR);
      __countTypeSensor++;
    }
    else if (isAtuador(deviceConnected->uuid.c_str()))
    {
      Serial.println("[BLEServerService::connectMyDisp()]: Is Actuator ");
      disp.setTypeDisp(TYPE_ACTUATOR);
      __countTypeActuator++;
    }

    __devicesMapped.insert(std::make_pair(deviceConnected->uuid.c_str(), disp));
    Serial.println("[BLEServerService::connectMyDisp()]: connection saved. finalized");

    if (deviceConnected->pClient->isConnected())
      deviceConnected->pClient->disconnect();

    free(deviceConnected->pClient);
    free(deviceConnected->pRemoteCharacteristic);
    free(deviceConnected->pRemoteService);
    delete deviceConnected;

    return true;
  }
  else
  {
    Serial.println("[BLEServerService::connectMyDisp()]: have problems in connection");

    free(deviceConnected->pClient);
    free(deviceConnected->pRemoteCharacteristic);
    free(deviceConnected->pRemoteService);
    delete deviceConnected;

    return false;
  }
}

void BLEServerService::sendMessageToActuator(String data)
{
  if (__actuatorConnected->pClient->isConnected())
    __actuatorConnected->pRemoteCharacteristic->writeValue(std::string(data.c_str()), false);
}

void BLEServerService::disconnectToActuator()
{
  delay(1000);

  if (__actuatorConnected->pClient->isConnected())
    __actuatorConnected->pClient->disconnect();

  delete __actuatorConnected;
}

bool BLEServerService::connectToActuator(String uuidDevice)
{
  Hardware disp;
  bool connected = false;

  for (auto item : __devicesMapped)
  {
    disp = item.second;

    Serial.println("[BLEServerService::connectToActuator()]: uuid mapped: " + disp.getUuid());
    if (uuidDevice.equals(disp.getUuid().c_str()))
    {
      Serial.println("[BLEServerService::connectToActuator()]: device found");
      __actuatorConnected = connectToDevice(disp.getBLEAdvertisedDevice(), false);

      Serial.println("[BLEServerService::connectToActuator()]: device found: ");
      Serial.println(__actuatorConnected->pClient->isConnected());
      if (__actuatorConnected->pClient->isConnected())
      {
        connected = true;
        break;
      }
    }
  }

  return connected;
}

void BLEServerService::continuousConnectionTask()
{
  // bool longTimeWithoutConnections = false;

  while (true)
  {
    vTaskDelay(pdMS_TO_TICKS(TIME_WAITING_CONNECTION));

    Serial.println("[BLEServerService::continuousConnectionTask()] Actual Time: " + String(millis()));

    if (!HTTP_REQUEST && !ENV_REQUEST)
    {
      __configuration.lockEnvVariablesMutex();

      __wfService.disconnect();

      newCicle();

      __wfService.connect();

      __configuration.unlockEnvVariablesMutex();
    }
  }
}

void BLEServerService::newCicle()
{
  BLEDeviceConnect *deviceConnected;
  vector<BLEDeviceConnect *> aux;
  bool isDeviceConected = false;
  Hardware disp;
  int count = 0;

  Serial.println("[BLEServerService::newCicle()] NEW CICLE");

  for (auto item : __devicesMapped)
  {
    if (__configuration.isDebug())
    {
      Serial.println("[BLEServerService::newCicle()] Receive Request: " + String(HTTP_REQUEST));
      Serial.println("[BLEServerService::newCicle()] Env Request: " + String(ENV_REQUEST));
      Serial.println("[BLEServerService::newCicle()] In Class: " + String(IN_CLASS));
    }

    if (HTTP_REQUEST || ENV_REQUEST)
    {
      if (!aux.empty())
      {
        closeConnections(aux);
        aux.clear();
      }

      Serial.println("[BLEServerService::newCicle()] Request Enabled or No Class");

      return;
    }

    disp = item.second;

    if (disp.getTypeDisp() == TYPE_SENSOR)
    {
      if (__configuration.isDebug())
      {
        Serial.println("[BLEServerService::newCicle()] UUID: " + String(disp.getUuid()));
        Serial.println("[BLEServerService::newCicle()] ADDRESS: " + String(disp.getBLEAdvertisedDevice()->getAddress().toString().c_str()));
      }

      deviceConnected = connectToDevice(disp.getBLEAdvertisedDevice(), false);

      if (deviceConnected->pClient->isConnected())
      {
        String data = "GET_DATA";
        deviceConnected->pRemoteCharacteristic->writeValue(data.c_str(), data.length());
        isDeviceConected = true;
      }

      aux.push_back(deviceConnected);

      count++;

      if ((count % 3) == 0 || __countTypeSensor == count)
      {
        if (isDeviceConected)
          timer();

        closeConnections(aux);
        aux.clear();
      }
    }
  }
}

void BLEServerService::closeConnections(vector<BLEDeviceConnect *> aux)
{
  for (auto deviceCon : aux)
  {
    Serial.println();
    Serial.print("[BLEServerService::closeConnections()]: ");
    Serial.println(deviceCon->pClient->getPeerAddress().toString().c_str());

    delay(300);
    if (deviceCon->pClient->isConnected())
      deviceCon->pClient->disconnect();

    delay(300);
    free(deviceCon->pClient);
    free(deviceCon->pRemoteCharacteristic);
    free(deviceCon->pRemoteService);

    delete deviceCon;
  }
}

vector<String> BLEServerService::getSensors()
{
  return __sensors;
}

vector<struct HardwareRecord> BLEServerService::getActuators()
{
  return __actuators;
}

void BLEServerService::setCountTypeSensor(int count)
{
  __countTypeSensor = count;
}

void BLEServerService::setCounttypeActuator(int count)
{
  __countTypeActuator = count;
}

int BLEServerService::getCountTypeSensor()
{
  return __countTypeSensor;
}

int BLEServerService::getCounttypeActuator()
{
  return __countTypeActuator;
}

void BLEServerService::addSensor(String sensor)
{
  __sensors.push_back(sensor);
}

void BLEServerService::addActuator(HardwareRecord act)
{
  __actuators.push_back(act);
}

void BLEServerService::timer()
{
  unsigned long tempoLimite = millis() + TIME_CONNECTION;

  while (millis() <= tempoLimite && !HTTP_REQUEST && !ENV_REQUEST)
  {

    if ((millis() % 5000) == 0)
    {
      Serial.println();
      Serial.print("[BLEServerService::timer()]: MARCAÇÃO TEMPORAL: ");
      Serial.println();
      Serial.print("[BLEServerService::timer()]: tempo atual: ");
      Serial.println(millis());
      Serial.print("[BLEServerService::timer()]: tempo limite: ");
      Serial.println(tempoLimite);
      Serial.println();
    }
  };
}

void BLEServerService::startTaskBLEImpl(void *_this)
{
  BLEServerService *bleSettings = (BLEServerService *)_this;
  bleSettings->continuousConnectionTask();
}

void BLEServerService::startTaskBLE()
{
  xTaskCreate(this->startTaskBLEImpl, "Task", 8192, this, 8, NULL);
}