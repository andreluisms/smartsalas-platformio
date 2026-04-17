#include "Global.h"
#include "Controller.h"
#include "Config.h"

#define RELE 19

HardwareRecord hardware;
WiFiService wiFiService;
Controller controller;
String master = "";

unsigned long lastLoopLogMs = 0;
const unsigned long LOOP_LOG_INTERVAL_MS = 5000;

bool SEND_DATA = false;
String COMMAND;
const uint16_t kIrLed = 12;
IRsend irsend(kIrLed); 

void setup() {

	Serial.begin(115200);
  delay(300);
  Serial.println("[ATUADOR][INIT] Boot iniciado");
  pinMode(RELE, OUTPUT);
	irsend.begin();
	bool init = false;

	Serial.println("[ATUADOR][INIT] Conectando no Wi-Fi...");
	wiFiService.connect();
	Serial.println("[ATUADOR][INIT] Wi-Fi conectado");

	do {
		if ( controller.start(hardware) ) {
			if ( controller.registerHardware(hardware) ) {
				if(controller.getMaster(hardware, master))
				{
					Serial.println("[ATUADOR][INIT] Master: " + master);
					init = true;
				}
				controller.setHardwareConfig(hardware);

			}
		}
	} while( !init ); 

	Serial.println("[ATUADOR][INIT] UUID de hardware (backend): " + hardware.uuid);
	Serial.println("[ATUADOR][INIT] UUID BLE servico: " + String(SERVICEUUID));
	Serial.println("[ATUADOR][INIT] UUID BLE caracteristica: " + String(CHARACTERISTICUUID));

	wiFiService.disconnect();
	Serial.println("[ATUADOR][INIT] Configuracao concluida, iniciando BLE");

    controller.setupBLEClient("ESP_ATUADOR", ATUADOR);  
}

void loop() {
  //Serial.println((SEND_DATA));
  if(SEND_DATA) {
		Serial.println("[ATUADOR][CMD] Comando recebido, executando...");
    controller.ExecuteCommand(COMMAND);
    SEND_DATA = false;
		Serial.println("[ATUADOR][CMD] Comando executado");
  }

	if (millis() - lastLoopLogMs >= LOOP_LOG_INTERVAL_MS) {
		lastLoopLogMs = millis();
		Serial.println("[ATUADOR][STATUS] Aguardando comandos BLE");
	}

	delay(800);
}