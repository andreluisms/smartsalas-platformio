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
volatile bool BLE_BUSY = false;

static bool isCanonicalUuid(const String &value) 
{
  if (value.length() != 36)
    return false;

  for (int i = 0; i < value.length(); i++)
  {
    const char c = value.charAt(i);
    const bool hyphenPos = (i == 8 || i == 13 || i == 18 || i == 23);

    if (hyphenPos)
    {
      if (c != '-')
        return false;
      continue;
    }

    const bool isHex = (c >= '0' && c <= '9') ||
                       (c >= 'a' && c <= 'f') ||
                       (c >= 'A' && c <= 'F');

    if (!isHex)
      return false;
  }

  return true;
}

static String sanitizeBleText(const std::string &raw)
{
  String out = "";

  for (char c : raw)
  {
    // Accept only printable ASCII to avoid binary BLE payload residue.
    if (c >= 32 && c <= 126)
      out += c;
  }

  out.trim();
  out.toLowerCase();

  return out;
}

static void releaseDeviceConnect(BLEDeviceConnect *device)
{
  // Protege contra chamadas com ponteiro invalido.
  if (device == nullptr)
    return;

  // Se ainda houver conexao BLE ativa, encerra antes de destruir o wrapper.
  // Isso evita conexoes presas no stack BLE.
  if (device->pClient != nullptr && device->pClient->isConnected())
    device->pClient->disconnect();

  // Esses ponteiros sao de propriedade interna do NimBLE.
  // Nao usar free()/delete aqui para evitar corrupcao de heap.
  // Apenas anula as referencias locais para impedir uso apos liberacao.
  device->pRemoteCharacteristic = nullptr;
  device->pRemoteHardwareCharacteristic = nullptr;
  device->pRemoteService = nullptr;
  device->pClient = nullptr;

  // Libera somente a estrutura BLEDeviceConnect criada com new.
  delete device;
} 

BLEServerService::BLEServerService()
{
  __countTypeSensor = 0;
  __countTypeActuator = 0;
}

void BLEServerService::notifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)
{
  if (isNotify)
  {
    String data = String(((char *)pData));
    // Serial.print("[BLEServerService::notifyCallback()] Callback pela caracteristica: ");
    // Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    // Serial.print(" de tamanho " + length);
    // Serial.println();
    // Serial.print("[BLEServerService::notifyCallback()] Dado: ");
    // Serial.println(data.substring(0, length));
    // Serial.println("[BLEServerService::notifyCallback()] Payload BLE recebido com sucesso");
    if (__configuration.isDebug())
      Serial.println(String("[BLEServerService::notifyCallback()] Notificacao BLE recebida (") + String(length) + " bytes)");

    if (HTTP_REQUEST)
    {
      HTTP_RECEIVED_DATA = true;
      HTTP_MESSAGE = data;
    }
    else
    {
      ENV_RECEIVED_DATA = true;
      ENV_MESSAGE = data;
    }
  }
}

BLEDeviceConnect *BLEServerService::connectToDevice(BLEAdvertisedDevice *myDevice, bool validateConnection)
{
  BLEDeviceConnect *device = new BLEDeviceConnect();

  device->deviceFound = true;

  device->pClient = BLEDevice::createClient();

  bool ok = device->pClient->connect(myDevice);

  if (!ok)
  {
    device->deviceFound = false;
    return device;
  }

  delay(200);

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

  device->pRemoteHardwareCharacteristic = device->pRemoteService->getCharacteristic(HARDWARE_UUID_CHARACTERISTIC_UUID); //modificado

  if (device->pRemoteHardwareCharacteristic == nullptr)
  {
    Serial.print("[BLEServerService::connectToDevice()]: Falhou em encontrar a caracteristica de hardware UUID: ");
    Serial.println(HARDWARE_UUID_CHARACTERISTIC_UUID.toString().c_str());

    device->pClient->disconnect();
    device->deviceFound = false;

    return device;
  }

  // O atuador expõe o UUID de hardware em uma caracteristica dedicada.
  // Ler aqui separa identidade do equipamento do canal normal de dados.
  if (device->pRemoteHardwareCharacteristic->canRead())
  {
    String readUuid = "";
    const int maxAttempts = 3;

    for (int attempt = 0; attempt < maxAttempts; attempt++)
    {
      std::string remoteUuid = device->pRemoteHardwareCharacteristic->readValue();
      readUuid = sanitizeBleText(remoteUuid);

      if (isCanonicalUuid(readUuid))
        break;

      if (attempt < (maxAttempts - 1))
        delay(120);
    }

    device->uuid = readUuid;

    if (isCanonicalUuid(device->uuid))
    {
      // Serial.println("[BLEServerService::connectToDevice()]: UUID de hardware lido: " + device->uuid);
    }
    else if (device->uuid.length() > 0)
    {
      Serial.println("[BLEServerService::connectToDevice()]: valor lido da caracteristica nao eh UUID valido: " + device->uuid);
      device->uuid = "";
    }
    else
    {
      Serial.println("[BLEServerService::connectToDevice()]: UUID de hardware vazio");
    }
  }
  else
  {
    if (__configuration.isDebug())
      Serial.println("[BLEServerService::connectToDevice()]: caracteristica sem permissao de leitura");
  }

  device->pRemoteCharacteristic = device->pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);//adicionado para estabelecer a comunicação

  if (device->pRemoteCharacteristic == nullptr)
  {
    Serial.print("[BLEServerService::connectToDevice()]: Falhou em encontrar a caracteristica principal UUID: ");
    Serial.println(CHARACTERISTIC_UUID.toString().c_str());

    device->pClient->disconnect();
    device->deviceFound = false;

    return device;
  }

  if (validateConnection) //nao esta entrando aqui, essa etapa acontece na linha 613/618
  {
    delay(500);
    if (device->pRemoteCharacteristic->canNotify())
    {
      device->pRemoteCharacteristic->subscribe(true, notifyCallback);

      delay(200);

      device->pRemoteCharacteristic->writeValue("GET_DATA");
    }
    else
    {
      Serial.println("[BLEServerService::connectToDevice()]: caracteristica nao suporta notify, desconectando...");
      device->pClient->disconnect();
      device->deviceFound = false;

      return device;
    }
  }

  if (device->pRemoteCharacteristic->canNotify())
    device->pRemoteCharacteristic->subscribe(true, notifyCallback);
  // device->pRemoteCharacteristic->registerForNotify(notifyCallback);

  if (__configuration.isDebug())
  {
    Serial.println(String("[BLEServerService::connectToDevice()]: dispositivo conectado ") + myDevice->getAddress().toString().c_str());
  }

  return device;
}

void BLEServerService::initBLE()
{
  if (__configuration.isDebug())
    Serial.println("[BLEServerService::initBLE()] Iniciando configuracoes BLE");
  BLEDevice::init("ESP32_CONTROLLER");
}

void BLEServerService::activeBLEScan()
{
  __pBLEScan = BLEDevice::getScan();
  __pBLEScan->setInterval(1349);
  __pBLEScan->setWindow(449);
  __pBLEScan->setActiveScan(true);
  if (__configuration.isDebug())
    Serial.println("[BLEServerService::activeBLEScan()] Scan Ativo");
}

void BLEServerService::scanDevices()
{
  if (__devicesMapped.size() == 0)
  {
    for (auto device : __filteredDevices)
      delete device;

    __filteredDevices.clear();
  }

  BLEScanResults foundDevices = __pBLEScan->start(5, false);

  for (int i = 0; i < foundDevices.getCount(); i++)
    __filteredDevices.push_back(new BLEAdvertisedDevice(foundDevices.getDevice(i)));

  // BLEAdvertisedDevice* disp = NULL;
  for (auto item : __filteredDevices)
  {
    BLEAdvertisedDevice *disp = item;
    if (__configuration.isDebug())
      Serial.println(String("[BLEServerService::scanDevices()]: ") + disp->getAddress().toString().c_str());
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
  const size_t expectedDevices = __sensors.size() + __actuators.size();

  Serial.println("[BLEServerService::populateMap()]: Dispositivos esperados pela API - sensores: " + String(__sensors.size()) + ", atuadores: " + String(__actuators.size()));

  for (auto disp : __filteredDevices)
  {
    if (expectedDevices > 0 && __devicesMapped.size() >= expectedDevices)
      break;

    if (disp->haveServiceUUID() && disp->isAdvertisingService(SERVICE_UUID))
    {
      // bool deviceConnected = false;
      int index = 0, MAX = 4;
      bool mapped = false; 

      do
      {
        if (connectMyDisp(disp))
        {
            mapped = true;
          break;
        }

        index++;
      } while (index < MAX);

      if (mapped)
        Serial.println("[BLEServerService::populateMap()]: Device mapped");
      else
        if (__configuration.isDebug())
          Serial.println("[BLEServerService::populateMap()]: Device found, but mapping failed");
    }
    else if (__configuration.isDebug())
    {
      Serial.println("[BLEServerService::populateMap()]: Device no listed");
    }
  }

  if (__devicesMapped.size() == 0)
    Serial.println("[BLEServerService::populateMap()]: no devices found");

  Serial.println("[BLEServerService::populateMap()]: Dispositivos mapeados: " + String(__devicesMapped.size()) + "/" + String(expectedDevices));
}

bool BLEServerService::isSensor(String uuid)
{
  uuid.trim();
  uuid.toLowerCase();

  for (auto sensorUuid : __sensors)
  {
    sensorUuid.trim();
    sensorUuid.toLowerCase();

    if (sensorUuid.equals(uuid))
      return true;
  }

  return false;
}

bool BLEServerService::isAtuador(String uuid)
{
  uuid.trim();
  uuid.toLowerCase();

  for (auto item : __actuators)
  {
    String actuatorUuid = item.uuid;
    actuatorUuid.trim();
    actuatorUuid.toLowerCase();

    if (actuatorUuid.equals(uuid))
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

    if (uuid.equals(disp.getUuid().c_str()) && disp.getTypeDisp() == typeDisp)
    {
      return true;
    }
  }

  if (typeDisp == TYPE_ACTUATOR && __devicesMapped.size() == 1 && __actuators.size() == 1 && uuid.equals(__actuators[0].uuid))
  {
    Serial.println("[BLEServerService::isSensorListed()]: fallback para atuador unico ativo");
    return true;
  }

  return false;
}

bool BLEServerService::connectMyDisp(BLEAdvertisedDevice *device)
{
  // During bootstrap mapping, only validate connectivity/UUID.
  // Runtime data collection is handled by newCicle().
  BLEDeviceConnect *deviceConnected = connectToDevice(device, false); //alterando de true pra false pra teste
  String bleAddress = String(device->getAddress().toString().c_str());

  if (deviceConnected->deviceFound)
  {
    if (deviceConnected->uuid.length() == 0) 
    {
      if (__configuration.isDebug())
        Serial.println("[BLEServerService::connectMyDisp()]: empty UUID from device, skipping mapping");

      releaseDeviceConnect(deviceConnected);

      return false;
    }

    String mappedUuid = deviceConnected->uuid;

    if (__devicesMapped.find(mappedUuid.c_str()) != __devicesMapped.end())
    {
      if (__configuration.isDebug())
      {
        Serial.println("[BLEServerService::connectMyDisp()]: UUID ja mapeado, mantendo primeiro dispositivo: " + mappedUuid);
        Serial.println("[BLEServerService::connectMyDisp()]: endereco BLE duplicado ignorado: " + bleAddress);
      }

      releaseDeviceConnect(deviceConnected);

      return true;
    }

    Hardware disp;
    disp.setBLEAdvertisedDevice(device);
    disp.setUuid(mappedUuid.c_str());

    bool recognizedType = false; 
    String mappedType = "";

    if (isSensor(deviceConnected->uuid.c_str()))
    {
      disp.setTypeDisp(TYPE_SENSOR);
      __countTypeSensor++;
      recognizedType = true;
      mappedType = "SENSOR";
    }
    else if (isAtuador(mappedUuid))
    {
      disp.setTypeDisp(TYPE_ACTUATOR);
      __countTypeActuator++;
      recognizedType = true;
      mappedType = "ATUADOR";
    }

    if (!recognizedType)
    {
      if (__configuration.isDebug())
      {
        Serial.println("[BLEServerService::connectMyDisp()]: UUID nao reconhecido entre sensores/atuadores associados");
        Serial.println("[BLEServerService::connectMyDisp()]: endereco BLE ignorado: " + bleAddress);
        Serial.println("[BLEServerService::connectMyDisp()]: UUID lido: " + mappedUuid);
      }

      releaseDeviceConnect(deviceConnected);

      return false;
    }

    __devicesMapped.insert(std::make_pair(mappedUuid.c_str(), disp));
    if (__configuration.isDebug())
    {
      Serial.println("[BLEServerService::connectMyDisp()]: dispositivo mapeado");
      Serial.println("[BLEServerService::connectMyDisp()]: tipo: " + mappedType);
      Serial.println("[BLEServerService::connectMyDisp()]: endereco BLE: " + bleAddress);
      Serial.println("[BLEServerService::connectMyDisp()]: UUID: " + mappedUuid);
    }

    releaseDeviceConnect(deviceConnected);

    return true;
  }
  else
  {
    if (__configuration.isDebug())
      Serial.println("[BLEServerService::connectMyDisp()]: have problems in connection");

    releaseDeviceConnect(deviceConnected);

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
  vTaskDelay(pdMS_TO_TICKS(1000));

  if (__actuatorConnected->pClient->isConnected())
    __actuatorConnected->pClient->disconnect();

  BLEDevice::deleteClient(__actuatorConnected->pClient);
  delete __actuatorConnected;
}

bool BLEServerService::connectToActuator(String uuidDevice)
{
  Hardware disp;
  bool connected = false;

  for (auto item : __devicesMapped)
  {
    disp = item.second;
    if (uuidDevice.equals(disp.getUuid().c_str()))
    {
      __actuatorConnected = connectToDevice(disp.getBLEAdvertisedDevice(), false);
      if (__actuatorConnected->pClient->isConnected())
      {
        connected = true;
        break;
      }
    }
  }

  if (!connected && __devicesMapped.size() == 1 && __actuators.size() == 1 && uuidDevice.equals(__actuators[0].uuid))
  {
    auto onlyMapped = __devicesMapped.begin();
    disp = onlyMapped->second;

    Serial.println("[BLEServerService::connectToActuator()]: fallback para atuador unico");
    Serial.println("[BLEServerService::connectToActuator()]: uuid requisitado: " + uuidDevice);
    Serial.println("[BLEServerService::connectToActuator()]: uuid mapeado: " + disp.getUuid());

    __actuatorConnected = connectToDevice(disp.getBLEAdvertisedDevice(), false);
    if (__actuatorConnected->pClient->isConnected())
      connected = true;
  }

  return connected;
}

void BLEServerService::continuousConnectionTask()
{
  while (true)
  {
    vTaskDelay(pdMS_TO_TICKS(TIME_WAITING_CONNECTION));
    // Serial.println("[BLEServerService::continuousConnectionTask()] Actual Time: " + String(millis()));

    if (!HTTP_REQUEST && !ENV_REQUEST)
    {
      BLE_BUSY = true;
      __configuration.lockEnvVariablesMutex();

      //__wfService.disconnect();

      newCicle();

      //__wfService.connect();

      __configuration.unlockEnvVariablesMutex();
      BLE_BUSY = false;
    }

    vTaskDelay(pdMS_TO_TICKS(TIME_WAITING_CONNECTION));
  }
}

void BLEServerService::newCicle()
{
  BLEDeviceConnect *deviceConnected;
  vector<BLEDeviceConnect *> aux;
  bool isDeviceConected = false;
  Hardware disp;
  int count = 0;

  if (__configuration.isDebug())
    Serial.println("[BLEServerService::newCicle()] Devices mapped: " + String(__devicesMapped.size()));

  for (auto item : __devicesMapped)
  {
    if (__configuration.isDebug())
    {
      Serial.println("[BLEServerService::newCicle()] HTTP_REQUEST=" + String(HTTP_REQUEST) + " ENV_REQUEST=" + String(ENV_REQUEST) + " IN_CLASS=" + String(IN_CLASS));
    }

    if (HTTP_REQUEST || ENV_REQUEST)
    {
      if (!aux.empty())
      {
        closeConnections(aux);
        aux.clear();
      }

      return;
    }

    disp = item.second;

    if (disp.getTypeDisp() == TYPE_SENSOR)
    {
      if (__configuration.isDebug())
      {
        Serial.println("[BLEServerService::newCicle()] Sensor: " + String(disp.getUuid()) + " @ " + String(disp.getBLEAdvertisedDevice()->getAddress().toString().c_str()));
      }

      // Serial.println("[BLEServerService::newCicle()] Attempting BLE connection to sensor UUID: " + String(disp.getUuid()));
      deviceConnected = connectToDevice(disp.getBLEAdvertisedDevice(), false);

      if (deviceConnected->pClient->isConnected())
      {
        String data = "GET_DATA";
        deviceConnected->pRemoteCharacteristic->writeValue(data.c_str(), data.length());
        if (__configuration.isDebug())
          Serial.println("[BLEServerService::newCicle()] GET_DATA enviado para sensor UUID: " + String(disp.getUuid()));
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
    if (__configuration.isDebug())
      Serial.println(String("[BLEServerService::closeConnections()]: ") + deviceCon->pClient->getPeerAddress().toString().c_str());

    delay(300);
    releaseDeviceConnect(deviceCon);//adicionado
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

int BLEServerService::getExpectedDevicesCount()
{
  return __sensors.size() + __actuators.size();
}

int BLEServerService::getMappedDevicesCount()
{
  return __devicesMapped.size();
}

bool BLEServerService::hasMappedAllExpectedDevices()
{
  int expectedDevices = getExpectedDevicesCount();

  return expectedDevices > 0 && getMappedDevicesCount() >= expectedDevices;
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
  unsigned long nextLog = millis();

  while (millis() <= tempoLimite && !HTTP_REQUEST && !ENV_REQUEST)
  {
    if (millis() >= nextLog)
    {
      Serial.println();
      Serial.print("[BLEServerService::timer()]: MARCAÇÃO TEMPORAL: ");
      Serial.println();
      Serial.print("[BLEServerService::timer()]: tempo atual: ");
      Serial.println(millis());
      Serial.print("[BLEServerService::timer()]: tempo limite: ");
      Serial.println(tempoLimite);
      Serial.println();
      nextLog = millis() + 5000;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
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
