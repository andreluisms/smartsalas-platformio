#include "BLEServerService.h"
#include "Controller.h"
#include "Config.h"
#include "Structs.h"
#include "WiFiService.h" 
#include "WiFi.h"

BLEServerService* bleConfig; 
HardwareRecord hardware;
Controller controller;
WiFiService wiFiService;


void setup() {
	
	Serial.begin(115200);
  delay(1000);
	bool init = false;

  wiFiService.connect();

  //adicionando logs para ajudar no debug
  Serial.println("[CONTROLADOR][INIT] Wi-Fi conectado");
  Serial.println("[CONTROLADOR][INIT] MAC: " + WiFi.macAddress());
  Serial.println("[CONTROLADOR][INIT] IP: " + WiFi.localIP().toString());
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

  Serial.println("[CONTROLADOR][INIT] Hardware ID: " + String(hardware.id));
  Serial.println("[CONTROLADOR][INIT] Configuracao carregada");

  // Configure BLE Service
  controller.setupBLEServer();
  Serial.println("[CONTROLADOR][INIT] BLE configurado");
  controller.startBLETaskServer();	
  Serial.println("[CONTROLADOR][INIT] Task do servidor BLE iniciada");

  // Configure Http Service
  controller.startTaskHttp();

  // Configure Environment Variables Service
  controller.setupEnvironmentVariables();
  Serial.println("[CONTROLADOR][INIT] Sistema pronto");
}

void loop() {
  controller.environmentVariablesContinuousValidation(); 
}