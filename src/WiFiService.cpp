#include "Config.h"
#include "WiFiService.h"

// Construtor da classe WiFiService (vazio, sem inicializações)
WiFiService::WiFiService(){}

void WiFiService::connect(){   
    // Aguarda 4 segundos antes de tentar conectar
    // Útil para dar tempo do ESP inicializar corretamente
    delay(4000);
    
    // Contador de tentativas de conexão
    int attempt = 0;

    // Inicia a conexão Wi-Fi usando SSID e senha vindos do Config
    // c_str() converte String para const char*, exigido pela API do WiFi
    WiFi.begin(config.getSSID().c_str(), config.getPassword().c_str());



    // Enquanto:
    //  - não estiver conectado ao Wi-Fi
    //  - e não tiver excedido o número máximo de tentativas
    while (WiFi.status() != WL_CONNECTED && attempt < config.getWifiFailAttempts()){
        
        // Liga o LED de status, e aguarda 1 segundo
        digitalWrite(config.getLedStatus(), HIGH);
        delay(1000);

        // Desliga o LED (efeito de piscar)
        digitalWrite(config.getLedStatus(), LOW); 
        
        // Incrementa o número de tentativas
        attempt += 1;

        // Se modo debug habilitado, imprimir status da tentativa e info do ponto de acesso
        if (config.isDebug())
            Serial.println("[WiFiService::connect()] Tentativa: " + String(attempt) + ", SSID: "  + config.getSSID() + ", password: " + config.getPassword());
    }

    // Se não conectou e atingiu o limite máximo de tentativas
    if(WiFi.status() != WL_CONNECTED && attempt == config.getWifiFailAttempts()) {
      Serial.println(String("[WiFiService::connect()] Excedido o número de tentativas de conexão ao Wifi: ") + config.getWifiFailAttempts() + ". reiniciando ESP... ");
      ESP.restart();
    }

    // Mantém LED ligado indicando conexão bem-sucedida
    digitalWrite(config.getLedStatus(), HIGH);
    
    // Se conectou com sucesso e debug habilitado
    if (WiFi.status() == WL_CONNECTED && config.isDebug()){
        // Obtém o IP atribuído pelo roteador
        IPAddress ip = WiFi.localIP();
        Serial.println("[WiFiService::connect()] Conectado a rede: " + config.getSSID() + ", IP: " + ip);
//        Serial.println(ip);
    }
   
    return;
}

void WiFiService::disconnect()  {
    // Desconecta o Wi-Fi
    WiFi.disconnect();

    // Apaga o LED de status
    digitalWrite(config.getLedStatus(), LOW); 

    // Se debug habilitado, informa desconexao.
    if (config.isDebug())
        Serial.println("[WiFiService::disconnect()] Desconectado da rede: " + config.getSSID());
    
    return;
}
