#include "EnvironmentVariablesService.h"


String __currentTime;
struct Monitoramento EnvironmentVariablesService::__monitoringConditioner;
struct Monitoramento EnvironmentVariablesService::__monitoringLight;
vector<struct Reserva> EnvironmentVariablesService::__reservations; 
HardwareRecord EnvironmentVariablesService::__hardware; 
String __startTimeLoadReservations;
String __endTimeLoadReservations;
bool EnvironmentVariablesService::__hasMovement;
unsigned long EnvironmentVariablesService::__lastTimeAttended;
unsigned long EnvironmentVariablesService::__lastTimeLoadReservations;
BLEServerService* __bleServerConfig;
HTTPService __httpRequestService;
WiFiService __wifiService;
UtilsService __utilsService;
Config __config;

EnvironmentVariablesService::EnvironmentVariablesService()
{
    __startTimeLoadReservations  = "00:05:00";
    __endTimeLoadReservations    = "00:10:00";
    __hasMovement = false;
}

void EnvironmentVariablesService::initEnvironmentVariables() 
{
    __monitoringConditioner = __httpRequestService.getMonitoringByIdSalaAndEquipamento("CONDICIONADOR");
    __monitoringLight = __httpRequestService.getMonitoringByIdSalaAndEquipamento("LUZES");
    __reservations = __httpRequestService.getReservationsToday();
    __lastTimeLoadReservations = millis();
    __lastTimeAttended = millis();

    if (__config.isDebug())
    {
        Serial.println("[CTRL][ENV] initEnvironmentVariables concluido");
        Serial.println("[CTRL][ENV] Monitoramento AC: id=" + String(__monitoringConditioner.id) +
                       " | estado=" + String(__monitoringConditioner.estado) +
                       " | equipamentoId=" + String(__monitoringConditioner.equipamentoId));
        Serial.println("[CTRL][ENV] Monitoramento LZ: id=" + String(__monitoringLight.id) +
                       " | estado=" + String(__monitoringLight.estado) +
                       " | equipamentoId=" + String(__monitoringLight.equipamentoId));
        Serial.println("[CTRL][ENV] Reservas carregadas: " + String(__reservations.size()));
    }
}

unsigned long EnvironmentVariablesService::getLastTimeAttended() 
{
    return __lastTimeAttended;
}

void EnvironmentVariablesService::setLastTimeAttended(unsigned long time) 
{
    __lastTimeAttended = time;
}

std::vector<struct Reserva> EnvironmentVariablesService::getReservations()
{
    return __reservations;
}

void EnvironmentVariablesService::setReservations(std::vector<struct Reserva> reservations)
{
    __reservations = reservations;
}

struct HardwareRecord EnvironmentVariablesService::getHardware()
{
    return __hardware;
}

void EnvironmentVariablesService::setHardware(HardwareRecord hardware)
{
    __hardware = hardware;
}

struct Monitoramento EnvironmentVariablesService::getMonitoringLight()
{
  return __monitoringLight;
}

void EnvironmentVariablesService::setMonitoringLight(struct Monitoramento monitoring)
{
  __monitoringLight = monitoring;
}

struct Monitoramento EnvironmentVariablesService::getMonitoringConditioner()
{
  return __monitoringConditioner;
}

void EnvironmentVariablesService::setMonitoringConditioner(struct Monitoramento monitoring)
{
  __monitoringConditioner = monitoring;
}

unsigned long EnvironmentVariablesService::getLastTimeLoadReservations()
{
  return __lastTimeLoadReservations;
}

void EnvironmentVariablesService::setLastTimeLoadReservations(unsigned long time)
{
  __lastTimeLoadReservations = time;
} 

void EnvironmentVariablesService::sendDataToActuator(String uuid, String message)
{
  bool dispConnected = __bleServerConfig->connectToActuator(uuid);
                
  if(dispConnected)
  {
    ENV_RECEIVED_DATA = false;
    ENV_MESSAGE = "";

    std::vector<String> subStrings = __utilsService.splitPayload(message, MAX_LENGTH_PACKET);

    for (const String& packet: subStrings)
    {
      __bleServerConfig->sendMessageToActuator(packet);
      vTaskDelay(pdMS_TO_TICKS(75));
    }
        
    awaitsReturn();
  }

  __bleServerConfig->disconnectToActuator();
   
  delay(2000);

  __utilsService.updateMonitoring(ENV_MESSAGE);

  ENV_RECEIVED_DATA = false;
  ENV_MESSAGE = ""; 
}

void EnvironmentVariablesService::sendDataToActuator(int typeEquipment, String message)
{
  String uuid = getUuidActuator(typeEquipment);

  if(!__bleServerConfig->isSensorListed(uuid, TYPE_ACTUATOR))
  {
    if(__config.isDebug())
      Serial.println("[EnvironmentVariablesService::sendDataToActuator()]: No matching actuator with this uuid: " + uuid);

    return;
  }

  __config.lock();

  ENV_REQUEST = true;

  delay(1000);
  
  sendDataToActuator(uuid, message);

  ENV_REQUEST = false;

  __config.unlock();
}

String EnvironmentVariablesService::getUuidActuator(int typeEquipment)
{
  String uuid = "";
  for(struct HardwareRecord r : __bleServerConfig->getActuators())
  {
    if(r.typeEquipment == typeEquipment)
      uuid = r.uuid;
  }

  return uuid;
}

/*
 * <descricao> Verifica se a sala está reservada no horário atual e retorna TRUE se estiver em horário de reserva <descricao/>
 */
bool EnvironmentVariablesService::getRoomDuringClassTime() {
  
  String horaInicio, horaFim;
  bool inClass = false;
  
  for (const Reserva& r: __reservations) {

    horaInicio = r.horarioInicio;
    horaFim = r.horarioFim;
    
    if (__currentTime >= horaInicio && __currentTime < horaFim)
      inClass = true;
  }

  return inClass;
}

/*
 * <descricao> Verifica se é para ligar os dispostivos (luzes e ar) de acordo com as 
 * infomacoes obtidas dos modulos de sensoriamento e dos dados das reservas da sala <descricao/>
 */
void EnvironmentVariablesService::turnOnManagedDevices() {
    if (__config.isDebug())
    {
      Serial.println("[CTRL][ENV] Avaliando acionamento: IN_CLASS=" + String(IN_CLASS) +
                     " | hasMovement=" + String(__hasMovement) +
                     " | AC.estado=" + String(__monitoringConditioner.estado) +
                     " | AC.id=" + String(__monitoringConditioner.id) +
                     " | AC.equipamentoId=" + String(__monitoringConditioner.equipamentoId) +
                     " | LZ.estado=" + String(__monitoringLight.estado) +
                     " | LZ.id=" + String(__monitoringLight.id) +
                     " | LZ.equipamentoId=" + String(__monitoringLight.equipamentoId));
    }
    
    if (IN_CLASS && __hasMovement) 
    {
      if(__config.isDebug())
        Serial.println("[CTRL][ENV] Condicao para ligar dispositivos satisfeita");

      if (!__monitoringConditioner.estado && __monitoringConditioner.id > 0 && __monitoringConditioner.equipamentoId > 0)
        turnOnConditioner();

      if (!__monitoringLight.estado && __monitoringLight.id > 0 && __monitoringLight.equipamentoId > 0)
        turnOnLight();

    }  
}

/*
 * <descricao> Verifica se é para desligar os dispostivos (luzes e ar) de acordo com as 
 * informacoes obtidas dos modulos de sensoriamento e dos dados das reservas da sala <descricao/>
 */
void EnvironmentVariablesService::turnOffManagedDevices() {

  bool longTimeWithoutMovement = (millis() - __lastTimeAttended) > TIME_TO_TURN_OFF;

  if (!IN_CLASS || (IN_CLASS && longTimeWithoutMovement)) 
  {
    if (__monitoringConditioner.estado && __monitoringConditioner.id > 0 && __monitoringConditioner.equipamentoId > 0) 
      turnOfConditioner();

    if (__monitoringLight.estado && __monitoringLight.id > 0 && __monitoringLight.equipamentoId > 0)
      turnOfLight();
  }
}

/*
 * <descricao> Executa o comando de ligar luzes e envia o status do monitoramento pra o servidor além de gravar a operação em log <descricao/>
 */
void EnvironmentVariablesService::turnOnConditioner(){

  __config.lockEnvVariablesMutex();

  if(__config.isDebug())
  {
    // Serial.print("[EnvironmentVariablesService::turnOnConditioner()]: ");
    // Serial.println(__monitoringConditioner.estado ? "true" : "false");
    Serial.println("[EnvironmentVariablesService::turnOnConditioner()]: LIGANDO CONDICIONADOR");
  }

  if(WiFi.status() != WL_CONNECTED)
    return;

  String codigos = __httpRequestService.getComandosIrByIdSalaAndOperacao(getUuidActuator(TYPE_CONDITIONER));

  //------------------------------------------------------    
  String payload = __utilsService.mountPayload("AC", "ON", codigos);
  sendDataToActuator(TYPE_CONDITIONER, payload);
  //------------------------------------------------------

  __config.unlockEnvVariablesMutex();

}

/*
 * <descricao> Executa o comando de desligar luzes e envia o status do monitoramento pra o servidor além de gravar a operação em log <descricao/>
 */
void EnvironmentVariablesService::turnOfConditioner(){
  
  __config.lockEnvVariablesMutex();

  if(__config.isDebug())
  {
    // Serial.print("[EnvironmentVariablesService::turnOfConditioner()]: ");
    // Serial.println(__monitoringConditioner.estado ? "true" : "false");
    Serial.println("[EnvironmentVariablesService::turnOfConditioner()]: DESLIGANDO CONDICIONADOR");
  }

  if(WiFi.status() != WL_CONNECTED)
    return;
    
  String codigos = __httpRequestService.getComandosIrByIdSalaAndOperacao(getUuidActuator(TYPE_CONDITIONER));

  //------------------------------------------------------    
  String payload = __utilsService.mountPayload("AC", "OFF", codigos);
  sendDataToActuator(TYPE_CONDITIONER, payload);
  //------------------------------------------------------    

  __config.unlockEnvVariablesMutex();

}

/*
 * <descricao> Executa o comando de ligar luzes e envia o status do monitoramento pra o servidor além de gravar a operação em log <descricao/>
 */
void EnvironmentVariablesService::turnOnLight(){
  
  __config.lockEnvVariablesMutex();

  if(__config.isDebug())
  {
    // Serial.print("[EnvironmentVariablesService::turnOnLight()]: ");
    // Serial.println(__monitoringLight.estado ? "true" : "false");
    Serial.println("[EnvironmentVariablesService::turnOnLight()]: LIGANDO LUZES");
  }

  // ----------------------------------------------------------
  String payload = __utilsService.mountPayload("LZ", "ON", "null");
  sendDataToActuator(TYPE_LIGHT, payload);  
  // ----------------------------------------------------------

  __config.unlockEnvVariablesMutex();
}

/*
 * <descricao> Executa o comando de desligar luzes e envia o status do monitoramento pra o servidor além de gravar a operação em log <descricao/>
 */
void EnvironmentVariablesService::turnOfLight(){

  __config.lockEnvVariablesMutex();

  if(__config.isDebug())
  {
    // Serial.print("[EnvironmentVariablesService::turnOfLight()]: ");
    // Serial.println(__monitoringLight.estado ? "true" : "false");
    Serial.println("[EnvironmentVariablesService::turnOfLight()]: DESLIGANDO LUZES");
  }
  
  // ----------------------------------------------------------
  String payload = __utilsService.mountPayload("LZ", "OFF", "null");
  sendDataToActuator(TYPE_LIGHT, payload);  
  // ----------------------------------------------------------

  __config.unlockEnvVariablesMutex();
}

void EnvironmentVariablesService::awaitsReturn()
{
  unsigned long tempoLimite = millis() + TIME_TO_AWAIT_RETURN;
  while(millis() <= tempoLimite && !ENV_RECEIVED_DATA)
    vTaskDelay(pdMS_TO_TICKS(100));
}

void EnvironmentVariablesService::checkTimeToLoadReservations()
{
  if(WiFi.status() != WL_CONNECTED)   
    return;

  String currentTime = __httpRequestService.getTime(GET_TIME);

  if(!currentTime.equals(""))
    __currentTime = currentTime;
  
  bool timeToLoadReservations = (millis() - __lastTimeLoadReservations) >= TIME_TO_LOAD;

  if (timeToLoadReservations)
  {
    __reservations = __httpRequestService.getReservationsToday();
    setLastTimeLoadReservations(millis());
  } 
}

void EnvironmentVariablesService::checkEnvironmentVariables()
{
  if (ENV_RECEIVED_DATA) 
  {
    if(__config.isDebug())
      Serial.println("[CTRL][ENV] ENV_MESSAGE bruto: " + ENV_MESSAGE);

    struct MonitoringRecord variables = deserealizeData(ENV_MESSAGE);

    if(__config.isDebug())
    {
      Serial.println("[CTRL][ENV] Monitoramento interpretado: hasPresent=" + variables.hasPresent +
                     " | temperature=" + String(variables.temperature));
    }

    if (variables.hasPresent == "S") 
    {
      __hasMovement = true;
      __lastTimeAttended = millis();
    } 
    else 
    {
      __hasMovement = false;
    }

    if(__config.isDebug())
      Serial.println("[CTRL][ENV] Estado interno atualizado: __hasMovement=" + String(__hasMovement) +
                     " | __lastTimeAttended=" + String(__lastTimeAttended));

    ENV_MESSAGE = "";
    ENV_RECEIVED_DATA = false; 
  }
}

struct MonitoringRecord EnvironmentVariablesService::deserealizeData(String message)
{
  struct MonitoringRecord environmentVariables = {"", 0.0};
  
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, message);
  
  if (!error)
  {
    environmentVariables.temperature = doc["temperature"].as<int>();
    environmentVariables.hasPresent = doc["hasPresent"].as<String>();
  }
  else if(__config.isDebug())
  {
    Serial.println("[EnvironmentVariablesService::deserealizeData()] Falha no parse JSON.......");
    Serial.println(error.f_str());
  }

  return environmentVariables;
}

void EnvironmentVariablesService::continuousValidation()
{
  if (BLE_BUSY) //controle para evitar que a tarefa de validação de variáveis de ambiente interfira em operações BLE críticas, como conexões ou transferências de dados
  {
    // if(__config.isDebug())
    //   Serial.println("[EnvironmentVariablesService::continuousValidation()]: BLE em andamento, pulando validacao de ambiente");
    vTaskDelay(pdMS_TO_TICKS(10000));
    return;
  }

  checkTimeToLoadReservations();

  IN_CLASS = getRoomDuringClassTime();

  if(__config.isDebug())
    Serial.println("[CTRL][ENV] Validacao continua: horaAtual=" + __currentTime +
                   " | IN_CLASS=" + String(IN_CLASS) +
                   " | reservasCarregadas=" + String(__reservations.size()));

  checkEnvironmentVariables();

  turnOffManagedDevices();
      
  turnOnManagedDevices();

  vTaskDelay(pdMS_TO_TICKS(10000));
}
