
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

    delay(2000);

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

    bool actuatorMapped = __bleConfiguration->isSensorListed(request.uuid, TYPE_ACTUATOR);
    if(!actuatorMapped) {
        Serial.println("[CONTROLADOR][HTTP_TASK] Atuador nao mapeado no controlador. Tentando novo scan BLE...");
        Serial.println("[CONTROLADOR][HTTP_TASK] UUID da solicitacao: " + request.uuid);

        std::vector<struct HardwareRecord> associated = __bleConfiguration->getActuators();
        Serial.println("[CONTROLADOR][ASSOCIADOS] Atuadores associados cadastrados: " + String(associated.size()));
        for (const HardwareRecord& act : associated) {
            Serial.println("[CONTROLADOR][ASSOCIADOS] UUID: " + act.uuid);
        }

        __bleConfiguration->activeBLEScan();
        __bleConfiguration->scanDevices();
        __bleConfiguration->stopScan();
        __bleConfiguration->populateMap();

        actuatorMapped = __bleConfiguration->isSensorListed(request.uuid, TYPE_ACTUATOR);
        if (!actuatorMapped) {
            Serial.println("[CONTROLADOR][HTTP_TASK] Atuador continua nao mapeado apos novo scan. Solicitacao mantida pendente.");
            __configAcess.unlock();
            return;
        }

        Serial.println("[CONTROLADOR][HTTP_TASK] Atuador mapeado apos novo scan BLE");
    }

    HTTP_REQUEST = true;

    vTaskDelay(1500/portTICK_PERIOD_MS);
    
    bool dispConnected = connectToActuator(request.uuid);
    bool bleConfirmed = false;
    String bleMessage = "";

    if(dispConnected){
        String payload = getMessageToSend(request);
        Serial.println("[CONTROLADOR][HTTP_TASK] Enviando comando ao atuador");

        std::vector<String> subStrings = __utils.splitPayload(payload, MAX_LENGTH_PACKET);

        for (const String& packet : subStrings){       
            if (__configAcess.isDebug()) {
                Serial.println("[CONTROLADOR][HTTP_TASK] Enviando pacote BLE");
            }
            __bleConfiguration->sendMessageToActuator(packet);
        }

        awaitsReturn();

        bleMessage = HTTP_MESSAGE;
        bleConfirmed = HTTP_RECEIVED_DATA && bleMessage.length() > 0 && !bleMessage.equals("NOT-AVALIABLE") && !bleMessage.equals("ERROR");

        if (bleConfirmed)
            Serial.println("[CONTROLADOR][HTTP_TASK] Confirmacao BLE positiva recebida");
        else
            Serial.println("[CONTROLADOR][HTTP_TASK] Sem confirmacao BLE positiva");
    }
    else {
        Serial.println("[CONTROLADOR][HTTP_TASK] Falha ao conectar BLE. Solicitacao nao sera finalizada");
    }

    __bleConfiguration->disconnectToActuator();
    
    HTTP_REQUEST = false;
    
    delay(2000);

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
      delay(1000);
      if (__configAcess.isDebug())
                Serial.println("[CONTROLADOR][HTTP_TASK] Aguardando retorno BLE... " + String(millis()));
  }    
}