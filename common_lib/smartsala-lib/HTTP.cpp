#include "HTTP.h"


HTTP http;

HTTP::HTTP(){}

String HTTP::request(String resource, String type, String params) const{
    
    // WiFiService wifi;
    int httpCode = 0;
    String response = "";
    String url = config.getUrl(); 
    
    resource.trim();
    
    type.trim();
    type.toLowerCase();
  
    if ( !(type.compareTo("put") == 0) && !(type.compareTo("post") == 0) && !(type.compareTo("get") == 0)){
        if(config.isDebug())
          Serial.println("[HTTP::request()] Tipo de requisição [" + type + "] inválido");

        return "Tipo de requisição inválido";
    }
        
    if (WiFi.status() == WL_CONNECTED) {
      
        HTTPClient http;
        // Serial.println("[HTTP] Conectado para requisicao");
      
        url.concat(resource);
        // Serial.println("[HTTP] Requisicao " + type + " -> " + url);
    
        http.begin(url.c_str());
        if (type.compareTo("post") == 0){

            http.addHeader("Content-Type", "application/json");          
            httpCode = http.POST(params);
            Serial.println("[HTTP::request()] Code: " + String(httpCode));

        }else if(type.compareTo("put") == 0){

            http.addHeader("Content-Type", "application/json");          
            httpCode = http.PUT(params);
            Serial.println("[HTTP::request()] Code: " + String(httpCode));        
        
        }else if(type.compareTo("get") == 0){
            httpCode = http.GET();
        }

        if (httpCode > 0) { 
            if(httpCode == 204){
              response = "[NO_CONTENT]: ";
              response.concat(httpCode);
            }
            else {
              response = http.getString();
            }
        }
        else{
            response = "[ERROR]";
            response.concat(httpCode);
        }
      
        if(config.isDebug())
        {
            // Serial.println("[HTTP] Finalizada com codigo " + String(httpCode) + " (resposta em " + String(response.length()) + " bytes)");
            Serial.println(String("[HTTP] ") + type + " " + resource + " -> " + String(httpCode) + " (" + String(response.length()) + " bytes)");
        }
      
        http.end();
    }else{
      
        if(config.isDebug())
            Serial.println("[HTTP] Sem Wi-Fi, requisicao nao executada");
      
    }

    return response;
}
