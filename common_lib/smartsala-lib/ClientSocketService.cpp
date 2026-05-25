#include "ClientSocketService.h"

Config configuration;
BLEServerService* __bleClientConfiguration; 
HTTPService __httpClientService;
EnvironmentVariablesService __env;
UtilsService __utilsClient;

ClientSocketService::ClientSocketService() {}

bool ClientSocketService::__messageReturned = false;
String ClientSocketService::__message = "";
WiFiServer ClientSocketService::__server(8088);

void ClientSocketService::initServer() {
      __server.begin();
}

String ClientSocketService::getMessage() {
    return  __message;
}

void ClientSocketService::setMessage(String message) {
    __message = message;
}

bool ClientSocketService::getMessageReturned() {
    return  __messageReturned;
}

void ClientSocketService::setMessageReturned(bool messageReturned) {
    __messageReturned = messageReturned;
}

/*
 * <descricao> Ouve requisicoes do cliente conecta via socket <descricao/>
 */
namespace {
  bool isHttpRequestLine(const String& line) {
    return line.startsWith("GET ") || line.startsWith("POST ") || line.startsWith("PUT ") || line.startsWith("OPTIONS ");
  }

  int parseContentLength(String header) {
    header.trim();
    if (!header.startsWith("Content-Length:"))
      return -1;

    header.remove(0, String("Content-Length:").length());
    header.trim();
    return header.toInt();
  }

  String readHttpBody(WiFiClient& client, int contentLength) {
    String body = "";
    if (contentLength <= 0)
      return body;

    body.reserve(contentLength);
    const size_t chunkSize = 128;

    while (body.length() < contentLength && client.connected()) {
      int remaining = contentLength - body.length();
      size_t toRead = remaining < (int)chunkSize ? (size_t)remaining : chunkSize;
      uint8_t buffer[chunkSize];
      size_t readBytes = client.readBytes(buffer, toRead);
      if (readBytes == 0)
        break;

      for (size_t i = 0; i < readBytes; i++)
        body += (char)buffer[i];
    }

    return body;
  }

  String extractJsonPayload(String payload) {
    payload.trim();
    int start = payload.indexOf('{');
    int end = payload.lastIndexOf('}');
    if (start < 0 || end < 0 || end <= start)
      return "";

    if (start == 0 && end == payload.length() - 1)
      return payload;

    return payload.substring(start, end + 1);
  }

  void sendHttpResponse(WiFiClient& client, int statusCode, const String& body) {
    String statusText = (statusCode == 200) ? "OK" : "Bad Request";
    client.print("HTTP/1.1 " + String(statusCode) + " " + statusText + "\r\n");
    client.print("Content-Type: text/plain\r\n");
    client.print("Connection: close\r\n");
    client.print("Content-Length: " + String(body.length()) + "\r\n\r\n");
    client.print(body);
  }

  bool isTruthy(String value) {
    value.trim();
    value.toLowerCase();
    return value == "true" || value == "1";
  }
}

void ClientSocketService::serverListener() {

    WiFiClient client;    
    
    while(true) {          
      /* 
       * ouvindo o cliente 
       */
      client = __server.available();
  
      if (client) {
  
        /*
         * Checando se o cliente está conectando ao server
         */
        while (client.connected()) {
  
          if (client.available()) {
            client.setTimeout(2000);
            String msg = client.readStringUntil('\n');
            msg.trim();

            if (msg.length() == 0)
              continue;

            bool isHttp = isHttpRequestLine(msg);
            String payload = msg;

            if (isHttp) {
                int contentLength = 0;
                while (client.connected()) {
                  String header = client.readStringUntil('\n');
                  header.trim();
                  if (header.length() == 0)
                    break;

                  int parsedLength = parseContentLength(header);
                  if (parsedLength >= 0)
                    contentLength = parsedLength;
                }

                payload = readHttpBody(client, contentLength);
            }

            payload = extractJsonPayload(payload);
            if (payload.length() == 0) {
                if (configuration.isDebug())
                  Serial.println("[ClientSocketService::serverListener()] payload invalido ou sem JSON");

                if (isHttp) {
                  sendHttpResponse(client, 400, "INVALID_REQUEST");
                  client.stop();
                }

                continue;
            }
            
            if (configuration.isDebug())
            {
                Serial.println("[ClientSocketService::serverListener()] mensagem recebida");
                Serial.println("[ClientSocketService::serverListener()] mensagem: " + payload);
            }

            MonitoringRequest request = deserealizeObject(payload);
            if (request.type.length() == 0) {
                if (configuration.isDebug())
                  Serial.println("[ClientSocketService::serverListener()] JSON invalido para solicitacao");

                if (isHttp) {
                  sendHttpResponse(client, 400, "INVALID_REQUEST");
                  client.stop();
                }

                continue;
            }

            request.type.trim();
            request.type.toUpperCase();
            request.acting = isTruthy(request.acting) ? "True" : "False";
            
            if (configuration.isDebug())
            {
                Serial.println("[ClientSocketService::serverListener()] type: " + request.type);
                Serial.println("[ClientSocketService::serverListener()] code: " + request.code);
                Serial.println("[ClientSocketService::serverListener()] uuid: " + request.uuid);
            }
            
            if (request.type == CONDICIONADOR || request.type == LUZES || request.type == "LUZ") { 
 
                //__bleClientConfiguration->setReceivedRequest(true);

                bool dispConnected = connectToActuator(request.uuid);
                
                if(dispConnected)
                {
                  String payload = getMessageToSend(request);
                  __bleClientConfiguration->sendMessageToActuator(payload);

                  awaitsReturn();

                  __bleClientConfiguration->disconnectToActuator();
                }
                
                //__bleClientConfiguration->setReceivedRequest(false);

                String responseBody = __messageReturned ? __message : "NOT-AVALIABLE";
                if (isHttp) {
                  sendHttpResponse(client, 200, responseBody);
                  client.stop();
                } else {
                  client.println(responseBody);
                }
 
                __utilsClient.updateMonitoring(__message);

                if (configuration.isDebug())
                {
                  Serial.println("[ClientSocketService::serverListener()] Resposta BLE");
                  Serial.println("[ClientSocketService::serverListener()] recebeu retorno: " + __messageReturned);
                  Serial.println("[ClientSocketService::serverListener()] mensagem: " + __message);
                }

                __messageReturned = false;
                __message = "";  

            }  else if(request.type == ATUALIZAR) {
                   
                __env.setReservations(__httpClientService.getReservationsToday());
                if (isHttp) {
                  sendHttpResponse(client, 200, "OK");
                  client.stop();
                } else {
                  client.println("OK");
                }
            } else {
                if (configuration.isDebug())
                  Serial.println("[ClientSocketService::serverListener()] tipo de solicitacao nao suportado: " + request.type);

                if (isHttp) {
                  sendHttpResponse(client, 400, "INVALID_REQUEST");
                  client.stop();
                }
            }
          }  
          delay(100);
        }
      }
      delay(500);
    }
}

MonitoringRequest ClientSocketService::deserealizeObject(String payload)
{
    MonitoringRequest request;
    request.type = "";
    request.code = "";
    request.uuid = "";
    request.acting = "";

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        if (configuration.isDebug()) {
            Serial.println("[ClientSocketService::deserealizeObject()] Falha no parse JSON");
            Serial.println(error.f_str());
        }
        return request;
    }
    
    request.type = doc["type"].as<const char *>();
    request.code = doc["code"].as<const char *>();
    request.uuid = doc["uuid"].as<const char *>();
    request.acting = doc["acting"].as<const char *>();
    
    return request;
}


bool ClientSocketService::connectToActuator(String uuidDevice) 
{
  bool deviceConnected = false;
  int i = 0;
  int count = 5;
            
  do
  { 
    i++;
    
    if (configuration.isDebug())
    {
      Serial.print("[ClientSocketService::connectToActuator()]: attempt number: ");
      Serial.println(i);
    }
    
    deviceConnected = __bleClientConfiguration->connectToActuator(uuidDevice);
    
    if(deviceConnected)
      break;
      
  } while(i < count);

  if( i >= count && !deviceConnected)
      Serial.println("[ClientSocketService::connectToActuator()]: device not found");

  return deviceConnected;
}

void ClientSocketService::awaitsReturn()
{
  
  unsigned long tempoLimite = millis() + 15000;
  while(millis() <= tempoLimite && !__messageReturned)
  { 
      delay(1000);
      if (configuration.isDebug())
      {    
        Serial.print("[ClientSocketService::awaitsReturn()] TIME AWAITS: ");
        Serial.println(millis());
      }
  }    
}

void ClientSocketService::startTaskWebSocketImpl(void* _this)
{
    ClientSocketService* socketService = (ClientSocketService*)_this;
    socketService->serverListener();
}

void ClientSocketService::startTaskWebSocket()
{
  xTaskCreate(this->startTaskWebSocketImpl, "serverListener", 8192, this, 5, NULL);
}

String ClientSocketService::getMessageToSend(MonitoringRequest request)
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

    return __utilsClient.mountPayload(typeEquipament, state, command);
}
00);
      if (configuration.isDebug())
      {    
        Serial.print("[ClientSocketService::awaitsReturn()] TIME AWAITS: ");
        Serial.println(millis());
      }
  }    
}

void ClientSocketService::startTaskWebSocketImpl(void* _this)
{
    ClientSocketService* socketService = (ClientSocketService*)_this;
    socketService->serverListener();
}

void ClientSocketService::startTaskWebSocket()
{
  xTaskCreate(this->startTaskWebSocketImpl, "serverListener", 8192, this, 5, NULL);
}

String ClientSocketService::getMessageToSend(MonitoringRequest request)
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

    return __utilsClient.mountPayload(typeEquipament, state, command);
}
