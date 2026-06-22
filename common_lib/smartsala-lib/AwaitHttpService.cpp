
#include "AwaitHttpService.h"

Config __configAcess;
BLEServerService* __bleConfiguration; 
HTTPService __httpService;
EnvironmentVariablesService __environment;
UtilsService __utils;
WiFiService __wifi;

AwaitHttpService::AwaitHttpService() {}

static bool isValidBleAck(const String& message)
{
    return message.equals(AC_ON) ||
           message.equals(AC_OFF) ||
           message.equals(LZ_ON) ||
           message.equals(LZ_OFF);
}

// Configurações de tempo para envio dos pacotes BLE e timeout para resposta BLE
static const TickType_t BLE_PACKET_GAP_MS = pdMS_TO_TICKS(75);
static const unsigned long HTTP_BLE_RESPONSE_TIMEOUT_MS = 20000;

void AwaitHttpService::startAwait()
{
    xTaskCreate(this->awaitSolicitation, "awaitSolicitation", 8192, this, 8, NULL);
}

void AwaitHttpService::awaitSolicitation(void* _this){
    std::vector<Solicitacao> solicitacao;
    bool lastWifiConnected = false;
    Serial.println("[CONTROLADOR][HTTP_TASK] Tarefa iniciada");
    
    while(true){
        bool wifiConnected = (WiFi.status() == WL_CONNECTED);

        if(wifiConnected && !lastWifiConnected){
            Serial.println("[CONTROLADOR][HTTP_TASK] Wi-Fi conectado");
        }

        if(!wifiConnected && lastWifiConnected){
            Serial.println("[CONTROLADOR][HTTP_TASK] Wi-Fi desconectado");
        }

        if(BLE_BUSY){ //controle para evitar que a tarefa de solicitação HTTP interfira em operações BLE críticas, como conexões ou transferências de dados
            Serial.println("[CONTROLADOR][HTTP_TASK] BLE em andamento, pulando polling HTTP");
        }
        else if(wifiConnected){
            
            solicitacao = __httpService.getSolicitacao(MONITORAMENTO);

            if (solicitacao.size() > 0) {
                Serial.println("[CONTROLADOR][HTTP_TASK] Solicitacoes pendentes: " + String(solicitacao.size()));
            }
            
            for (Solicitacao s : solicitacao){
                Serial.println("[CONTROLADOR][HTTP_TASK] Executando solicitacao ID " + String(s.id));
                executeSolicitation(s);
            }
        }

        lastWifiConnected = wifiConnected;
        
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

bool AwaitHttpService::connectToActuator(String uuidDevice) {
    Serial.println("[CONTROLADOR][HTTP_TASK] Conectando ao atuador: " + uuidDevice);
  bool deviceConnected = false;
  int i = 0;
  int count = 8;
            
  do{ 
    i++;
    
    if (__configAcess.isDebug())
    Serial.println("[CONTROLADOR][HTTP_TASK] Tentativa de conexao: " + String(i));
    
    deviceConnected = __bleConfiguration->connectToActuator(uuidDevice);
    
    if(deviceConnected)
      break;

    vTaskDelay(pdMS_TO_TICKS(2000));

  } while(i < count);

  if( i >= count && !deviceConnected)
    Serial.println("[CONTROLADOR][HTTP_TASK] Atuador nao encontrado");
    else if (deviceConnected)
        Serial.println("[CONTROLADOR][HTTP_TASK] Atuador conectado via BLE");

  return deviceConnected;
}

void AwaitHttpService::executeSolicitation(Solicitacao request) {
    __configAcess.lock();

    Serial.println("[CONTROLADOR][HTTP_TASK] Validando atuador para solicitacao ID " + String(request.id));

    String actuatorUuid = resolveActuatorUuid(request);
    if (actuatorUuid.length() == 0) {
        Serial.println("[CONTROLADOR][HTTP_TASK] Nao foi possivel resolver o UUID do atuador para a solicitacao.");
        Serial.println("[CONTROLADOR][HTTP_TASK] UUID da solicitacao: " + request.uuid);
        Serial.println("[CONTROLADOR][HTTP_TASK] Tipo da solicitacao: " + request.type);

        std::vector<struct HardwareRecord> associated = __bleConfiguration->getActuators();
        Serial.println("[CONTROLADOR][ASSOCIADOS] Atuadores associados cadastrados: " + String(associated.size()));
        for (const HardwareRecord& act : associated) {
            Serial.println("[CONTROLADOR][ASSOCIADOS] UUID: " + act.uuid + ", tipo equipamento: " + String(act.typeEquipment));
        }

        __configAcess.unlock();
        return;
    }

    if (!actuatorUuid.equals(request.uuid)) {
        Serial.println("[CONTROLADOR][HTTP_TASK] UUID da solicitacao nao corresponde ao UUID BLE do atuador.");
        Serial.println("[CONTROLADOR][HTTP_TASK] UUID da solicitacao: " + request.uuid);
        Serial.println("[CONTROLADOR][HTTP_TASK] UUID BLE resolvido: " + actuatorUuid);
    }

    bool actuatorMapped = __bleConfiguration->isSensorListed(actuatorUuid, TYPE_ACTUATOR);
    if(!actuatorMapped) {
        Serial.println("[CONTROLADOR][HTTP_TASK] Atuador nao mapeado no controlador. Tentando novo scan BLE...");
        Serial.println("[CONTROLADOR][HTTP_TASK] UUID da solicitacao: " + request.uuid);
        Serial.println("[CONTROLADOR][HTTP_TASK] UUID BLE esperado: " + actuatorUuid);

        std::vector<struct HardwareRecord> associated = __bleConfiguration->getActuators();
        Serial.println("[CONTROLADOR][ASSOCIADOS] Atuadores associados cadastrados: " + String(associated.size()));
        for (const HardwareRecord& act : associated) {
            Serial.println("[CONTROLADOR][ASSOCIADOS] UUID: " + act.uuid);
        }

        __bleConfiguration->activeBLEScan();
        __bleConfiguration->scanDevices();
        __bleConfiguration->stopScan();
        __bleConfiguration->populateMap();

        actuatorMapped = __bleConfiguration->isSensorListed(actuatorUuid, TYPE_ACTUATOR);
        if (!actuatorMapped) {
            Serial.println("[CONTROLADOR][HTTP_TASK] Atuador continua nao mapeado apos novo scan. Solicitacao mantida pendente.");
            __configAcess.unlock();
            return;
        }

        Serial.println("[CONTROLADOR][HTTP_TASK] Atuador mapeado apos novo scan BLE");
    }

    HTTP_REQUEST = true;

    vTaskDelay(1500/portTICK_PERIOD_MS);
    
    bool dispConnected = connectToActuator(actuatorUuid);
    bool bleConfirmed = false;
    String bleMessage = "";

    if(dispConnected){
        String payload = getMessageToSend(request);
        Serial.println("[CONTROLADOR][HTTP_TASK] Enviando comando ao atuador");

        HTTP_RECEIVED_DATA = false;
        HTTP_MESSAGE = "";

        std::vector<String> subStrings = __utils.splitPayload(payload, MAX_LENGTH_PACKET);

        for (const String& packet : subStrings){       
            if (__configAcess.isDebug()) {
                Serial.println("[CONTROLADOR][HTTP_TASK] Enviando pacote BLE");
            }
            __bleConfiguration->sendMessageToActuator(packet);
            vTaskDelay(BLE_PACKET_GAP_MS);
        }

        awaitsReturn();

        bleMessage = HTTP_MESSAGE;
        bleMessage.trim();
        bleConfirmed = HTTP_RECEIVED_DATA && isValidBleAck(bleMessage);

        if (bleConfirmed)
            Serial.println("[CONTROLADOR][HTTP_TASK] Confirmacao BLE positiva recebida");
        else {
            Serial.println("[CONTROLADOR][HTTP_TASK] Sem confirmacao BLE positiva");
            if (HTTP_RECEIVED_DATA)
                Serial.println("[CONTROLADOR][HTTP_TASK] Resposta BLE ignorada por formato invalido: " + bleMessage);
        }
    }
    else {
        Serial.println("[CONTROLADOR][HTTP_TASK] Falha ao conectar BLE. Solicitacao nao sera finalizada");
    }

    if (dispConnected)
        __bleConfiguration->disconnectToActuator();
    
    HTTP_REQUEST = false;
    
    vTaskDelay(pdMS_TO_TICKS(2000));

    if (bleConfirmed) {
        __utils.updateMonitoring(bleMessage);

        bool finalized = __httpService.putSolicitacao(request.id);
        if (finalized)
            Serial.println("[CONTROLADOR][HTTP_TASK] Solicitacao ID " + String(request.id) + " finalizada");
        else
            Serial.println("[CONTROLADOR][HTTP_TASK] Falha ao finalizar solicitacao ID " + String(request.id));
    } else {
        Serial.println("[CONTROLADOR][HTTP_TASK] Solicitacao ID " + String(request.id) + " mantida pendente (sem confirmacao BLE)");
    }

    if (__configAcess.isDebug())
    {
        Serial.println("[CONTROLADOR][HTTP_TASK] Resposta BLE recebida");
        Serial.println("[CONTROLADOR][HTTP_TASK] Retorno recebido: " + HTTP_RECEIVED_DATA);
        Serial.println("[CONTROLADOR][HTTP_TASK] Mensagem: " + bleMessage);
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
  unsigned long tempoLimite = millis() + HTTP_BLE_RESPONSE_TIMEOUT_MS;
  while(millis() <= tempoLimite && !HTTP_RECEIVED_DATA)
  { 
      vTaskDelay(pdMS_TO_TICKS(2000));
      if (__configAcess.isDebug())
                Serial.println("[CONTROLADOR][HTTP_TASK] Aguardando retorno BLE... " + String(millis()));
  }    
}
