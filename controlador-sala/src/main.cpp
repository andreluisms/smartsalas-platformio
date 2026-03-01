#include "BLEServerService.h"
#include "Controller.h"
#include "Config.h"
#include "Structs.h"
#include "WiFiService.h" 

BLEServerService* bleConfig; 
HardwareRecord hardware;
Controller controller;
WiFiService wiFiService;


String leituraSimples(bool ocultar){
  String resp = "";
  while (Serial.available()) Serial.read();
  while (true) {
      if (Serial.available()) {
          char c = Serial.read();
          if (c == '\r') continue;  
          if (c == '\n') {
            Serial.println();
            break;
          }
          if (c == 8 || c == 127) { 
              if (resp.length() > 0) {
                  resp.remove(resp.length() - 1);
                  Serial.print("\b \b");
              }
              continue;
          }

          resp += c;
          Serial.print(ocultar ? '*' : c);
      }
      vTaskDelay(pdMS_TO_TICKS(10));
  }
  return resp;
}

void configurarPA(){
    if (!config.isDefaultssid()){
      Serial.print("Entre com o SSID: ");
      config.setSSID(leituraSimples(false));
      Serial.print("Entre com o password: ");
      config.setPassword(leituraSimples(false));
    }
}


void setup() {
	
	Serial.begin(115200);
  delay(1000);
	bool init = false;

  configurarPA();
  wiFiService.connect();
	do {
    if ( controller.start(hardware) ) {
			if ( controller.registerHardware(hardware) ) {

        controller.setHardwareConfig(hardware);
        controller.fillHardwares(hardware);

				if ( controller.loadedDevices() )				
					init = true;

			}
		}
	} while( !init ); 

  // Configure BLE Service
  controller.setupBLEServer();
  controller.startBLETaskServer();	

  // Configure Http Service
  controller.startTaskHttp();

  // Configure Environment Variables Service
  controller.setupEnvironmentVariables();
}

void loop() {
  controller.environmentVariablesContinuousValidation(); 
}