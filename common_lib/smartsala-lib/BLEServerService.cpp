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
    Serial.print("[BLEServerService::notifyCallback()] Callback pela caracteristica: ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" de tamanho " + length);
    Serial.println();
    Serial.print("[BLEServerService::notifyCallback()] Dado: ");
    String data = String(((char *)pData));
    Serial.println(data.substring(0, length));
    Serial.println("[BLEServerService::notifyCallback()] Payload BLE recebido com sucesso");

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
  device->pRemoteHardwareCharacteristic = device->pRemoteService->getCharacteristic(HARDWARE_UUID_CHARACTERISTIC_UUID); //modificado

  if (device->pRemoteHardwareCharacteristic == nullptr)
  {
    Serial.print("[BLEServerService::connectToDevice()]: Falhou em encontrar a caracteristica de hardware UUID: ");
    Serial.println(HARDWARE_UUID_CHARACTERISTIC_UUID.toString().c_str());

    device->pClient->disconnect();
    device->deviceFound = false;

    return device;
  }

  Serial.println("[BLEServerService::connectToDevice()]: Encontrou a caracteristica de hardware");

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
      Serial.println("[BLEServerService::connectToDevice()]: UUID de hardware lido: " + device->uuid);
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

  Serial.println("[BLEServerService::connectToDevice()]: Encontrou nossa caracteristica principal");

  if (validateConnection) //nao esta entrando aqui, essa etapa acontece na linha 613/618
  {
    Serial.println("[BLEServerService::connectToDevice()]: Conexao valida");
    delay(500);
    if (device->pRemoteCharacteristic->canNotify())
    {
      Serial.println("[BLEServerService::connectToDevice()]: Ativando notify");
      device->pRemoteCharacteristic->subscribe(true, notifyCallback);

      delay(200);

      Serial.println("[BLEServerService::connectToDevice()]: Solicitando dados (GET_DATA)");
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
      bool mapped = false; 

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
        {
            mapped = true;
          break;
        }

        index++;
      } while (index < MAX);

      if (mapped)
        Serial.println("[BLEServerService::populateMap()]: Device mapped");
      else
        Serial.println("[BLEServerService::populateMap()]: Device found, but mapping failed");
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

  // During bootstrap mapping, only validate connectivity/UUID.
  // Runtime data collection is handled by newCicle().
  BLEDeviceConnect *deviceConnected = connectToDevice(device, false); //alterando de true pra false pra teste

  if (deviceConnected->deviceFound)
  {
    Serial.println("[BLEServerService::connectMyDisp()]: forming successfull a connection");

    if (deviceConnected->uuid.length() == 0) 
    {
      Serial.println("[BLEServerService::connectMyDisp()]: empty UUID from device, skipping mapping");

      releaseDeviceConnect(deviceConnected);

      return false;
    }

    Hardware disp;
    disp.setBLEAdvertisedDevice(device);
    disp.setUuid(deviceConnected->uuid.c_str());

    bool recognizedType = false; 

    if (isSensor(deviceConnected->uuid.c_str()))
    {
      Serial.println("[BLEServerService::connectMyDisp()]: Is Sensor ");
      disp.setTypeDisp(TYPE_SENSOR);
      __countTypeSensor++;
      recognizedType = true;
    }
    else if (isAtuador(deviceConnected->uuid.c_str()))
    {
      Serial.println("[BLEServerService::connectMyDisp()]: Is Actuator ");
      disp.setTypeDisp(TYPE_ACTUATOR);
      __countTypeActuator++;
      recognizedType = true;
    }

    if (!recognizedType)
    {
      Serial.println("[BLEServerService::connectMyDisp()]: UUID nao reconhecido entre sensores/atuadores associados");

      releaseDeviceConnect(deviceConnected);

      return false;
    }

    __devicesMapped.insert(std::make_pair(deviceConnected->uuid.c_str(), disp));
    Serial.println("[BLEServerService::connectMyDisp()]: connection saved. finalized");

    releaseDeviceConnect(deviceConnected);

    return true;
  }
  else
  {
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
      BLE_BUSY = true;
      __configuration.lockEnvVariablesMutex();

      __wfService.disconnect();

      newCicle();

      __wfService.connect();

      __configuration.unlockEnvVariablesMutex();
      BLE_BUSY = false;
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
  Serial.println("[BLEServerService::newCicle()] Devices mapped: " + String(__devicesMapped.size()));

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
      Serial.println("[BLEServerService::newCicle()] Early return because HTTP_REQUEST=" + String(HTTP_REQUEST) + " ENV_REQUEST=" + String(ENV_REQUEST));
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

      Serial.println("[BLEServerService::newCicle()] Attempting BLE connection to sensor UUID: " + String(disp.getUuid()));

      deviceConnected = connectToDevice(disp.getBLEAdvertisedDevice(), false);

      if (deviceConnected->pClient->isConnected())
      {
        String data = "GET_DATA";
        deviceConnected->pRemoteCharacteristic->writeValue(data.c_str(), data.length());
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
    Serial.println();
    Serial.print("[BLEServerService::closeConnections()]: ");
    Serial.println(deviceCon->pClient->getPeerAddress().toString().c_str());

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