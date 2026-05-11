
#include "AwaitHttpService.h"

Config __configAcess;
BLEServerService* __bleConfiguration; 
HTTPService __httpService;
EnvironmentVariablesService __environment;
UtilsService __utils;
WiFiService __wifi;

AwaitHttpService::AwaitHttpService() {}

void AwaitHttpService::startAwait()
{
    xTaskCreate(this->awaitSolicitation, "awaitSolicitation", 8192, this, 8, NULL);
}

void AwaitHttpService::awaitSolicitation(void* _this){
    std::vector<Solicitacao> solicitacao;
    
    while(true){
        if(WiFi.status() == WL_CONNECTED){
            if (__configAcess.isDebug())
                Serial.println("[AwaitHttpService::startAwait()] Inicio");

            solicitacao = __httpService.getSolicitacao(MONITORAMENTO);
            
            for (Solicitacao s : solicitacao){
                String requestType = s.type;
                requestType.trim();
                requestType.toUpperCase();

                if (requestType == CONDICIONADOR) {
                    String code = s.code;
                    bool acting = (s.acting == "True" || s.acting == "true" || s.acting == "1");

                    Serial.println("[AwaitHttpService::awaitSolicitation()] Roteando solicitacao de CONDICIONADOR");
                    Serial.println("[AwaitHttpService::awaitSolicitation()] code: " + code + ", acting: " + String(acting ? "True" : "False"));

                    processConditionerSolicitation(s, code, acting);
                }
                else if (requestType == LUZES || requestType == "LUZ") {
                    bool acting = (s.acting == "True" || s.acting == "true" || s.acting == "1");

                    Serial.println("[AwaitHttpService::awaitSolicitation()] Roteando solicitacao de LUZES");
                    Serial.println("[AwaitHttpService::awaitSolicitation()] acting: " + String(acting ? "True" : "False"));

                    processLightsSolicitation(s, acting);
                }
                else {
                    Serial.println("[AwaitHttpService::awaitSolicitation()] Tipo nao suportado no payload: " + s.type);
                }

                __httpService.putSolicitacao(s.id);
            }

            if (__configAcess.isDebug())
                Serial.println("[AwaitHttpService::startAwait()] Fim");
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

bool AwaitHttpService::connectToActuator(String uuidDevice) {
  Serial.println("[AwaitHttpService::connectToActuator()]: Atuador : " + uuidDevice);
  bool deviceConnected = false;
  int i = 0;
  int count = 8;
            
  do{ 
    i++;
    
    if (__configAcess.isDebug())
      Serial.print("[AwaitHttpService::connectToActuator()]: numero da tentativa: " + String(i));
    
    deviceConnected = __bleConfiguration->connectToActuator(uuidDevice);
    
    if(deviceConnected)
      break;

    vTaskDelay(pdMS_TO_TICKS(2000));

  } while(i < count);

  if( i >= count && !deviceConnected)
      Serial.println("[AwaitHttpService::connectToActuator()]: dispositivo nao encontrado");

  return deviceConnected;
}

void AwaitHttpService::executeSolicitation(Solicitacao request) {
    __configAcess.lock();

    request.uuid = resolveActuatorUuid(request);

    if (request.uuid == "")
    {
        Serial.println("[AwaitHttpService::executeSolicitation()] Atuador nao encontrado para tipo: " + request.type);
        __configAcess.unlock();
        return;
    }

    if(!__bleConfiguration->isSensorListed(request.uuid, TYPE_ACTUATOR)) {
        Serial.println("[AwaitHttpService::executeSolicitation()] Atuador nao mapeado para UUID: " + request.uuid + ", type: " + request.type);
        __configAcess.unlock();
        return; 
    }

    HTTP_REQUEST = true;

    vTaskDelay(1500/portTICK_PERIOD_MS);
    
    bool dispConnected = connectToActuator(request.uuid);

    if(dispConnected){
        String payload = getMessageToSend(request);
        Serial.println("[AwaitHttpService::executeSolicitation()] Enviando payload: " + payload);

        std::vector<String> subStrings = __utils.splitPayload(payload, MAX_LENGTH_PACKET);

        for (const String& packet : subStrings){       
            Serial.println("[AwaitHttpService::executeSolicitation()] Enviando packet: " + packet);
            __bleConfiguration->sendMessageToActuator(packet);
        }

        awaitsReturn();
    }

    __bleConfiguration->disconnectToActuator();
    
    HTTP_REQUEST = false;
    
    vTaskDelay(pdMS_TO_TICKS(2000));

    __utils.updateMonitoring(HTTP_MESSAGE);

    if (__configAcess.isDebug())
    {
        Serial.println("[AwaitHttpService::executeSolicitation()] Resposta BLE");
        Serial.println("[AwaitHttpService::executeSolicitation()] recebeu retorno: " + String(HTTP_RECEIVED_DATA));
        Serial.println("[AwaitHttpService::executeSolicitation()] mensagem: " + HTTP_MESSAGE);
    }

    HTTP_RECEIVED_DATA = false;
    HTTP_MESSAGE = "";  

    __configAcess.unlock();
}

String AwaitHttpService::resolveActuatorUuid(Solicitacao request)
{
    std::vector<HardwareRecord> actuators = __bleConfiguration->getActuators();
    String requestType = request.type;
    requestType.trim();
    requestType.toUpperCase();

    int expectedTypeEquipment = -1;
    if (requestType == CONDICIONADOR)
        expectedTypeEquipment = TYPE_CONDITIONER;
    else if (requestType == LUZES || requestType == "LUZ")
        expectedTypeEquipment = TYPE_LIGHT;
    else
        return "";

    String requestUuid = request.uuid;
    requestUuid.trim();
    bool hasPayloadUuid = (requestUuid.length() > 0 && requestUuid != "null" && requestUuid != "NULL");
    if (hasPayloadUuid)
    {
        for (const HardwareRecord& actuator : actuators)
        {
            if (actuator.uuid == requestUuid && actuator.typeEquipment == expectedTypeEquipment)
                return actuator.uuid;
        }
    }

    String uniqueUuid = "";
    int countByType = 0;
    for (const HardwareRecord& actuator : actuators)
    {
        if (actuator.typeEquipment == expectedTypeEquipment)
        {
            uniqueUuid = actuator.uuid;
            countByType++;
        }
    }

    if (countByType == 1)
        return uniqueUuid;

    return "";
}

void AwaitHttpService::processConditionerSolicitation(Solicitacao request, String code, bool acting)
{
    request.type = CONDICIONADOR;
    request.code = code;
    request.acting = acting ? "True" : "False";
    executeSolicitation(request);
}

void AwaitHttpService::processLightsSolicitation(Solicitacao request, bool acting)
{
    request.type = LUZES;
    request.code = "null";
    request.acting = acting ? "True" : "False";
    executeSolicitation(request);
}

String AwaitHttpService::getMessageToSend(Solicitacao request)
{
    String typeEquipament = "", state = "", command = "null";

    if(request.type == LUZES)
        typeEquipament = "LZ";
    else
    {
        typeEquipament = "AC";
        command = request.code;
    }

    if(request.acting == "True")
        state = "ON";
    else
        state = "OFF";

    return __utils.mountPayload(typeEquipament, state, command);
}

void AwaitHttpService::awaitsReturn()
{
  unsigned long tempoLimite = millis() + 15000;
  while(millis() <= tempoLimite && !HTTP_RECEIVED_DATA)
  { 
      vTaskDelay(pdMS_TO_TICKS(1000));
      if (__configAcess.isDebug())
        Serial.print("[AwaitHttpService::awaitsReturn()] TIME: " + millis());
  }    
}
