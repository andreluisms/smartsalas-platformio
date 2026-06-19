#include "Global.h"
#include "Controller.h"
#include "Config.h"
#include "DHT.h"

void DataMonitoring();

#define RELE 19

HardwareRecord hardware;
MonitoringRecord monitoringRecord;
WiFiService wiFiService;
Controller controller;
String master = "";

bool SEND_DATA = false;
String COMMAND;
const uint16_t kIrLed = 12;
IRsend irsend(kIrLed); 

DHT dht(4, DHT11);
float temperature;
const int portaPresenca = 14;

int qtdDetectouPresenca = 0;
bool estadoAnteriorPresenca = false;
unsigned long ultimoMovimentoMs = 0;
unsigned long ultimaLeituraPresencaMs = 0;
const unsigned long janelaPresencaMs = 30000;
const unsigned long intervaloLeituraPresencaMs = 100;
int qtdEventosMovimento = 0;

void setup() {

	Serial.begin(115200);
  pinMode(RELE, OUTPUT);
  pinMode(portaPresenca, INPUT);
	irsend.begin();
  dht.begin();
	bool init = false;

	wiFiService.connect();

	do {
		if ( controller.start(hardware) ) {
			if ( controller.registerHardware(hardware) ) {
				if(controller.getMaster(hardware, master))
				{
					Serial.print("master: ");
					Serial.println(master);
					init = true;
				}
				controller.setHardwareConfig(hardware);

			}
		}
	} while( !init ); 

	wiFiService.disconnect();

    controller.setupBLEClient("ESP_SENSOR", SENSOR);  
}

void loop() {

  handleBLEConnectionState();
  Serial.println("[INO]: data solicited ");
  
  bool leitura = digitalRead(portaPresenca);
  unsigned long agora = millis();
  temperature = dht.readTemperature();

  if ((agora - ultimaLeituraPresencaMs) >= intervaloLeituraPresencaMs) {
    if (leitura && !estadoAnteriorPresenca) {
      ultimoMovimentoMs = agora;
      qtdEventosMovimento++;
      Serial.print("[PIR][EVENTO] Movimento detectado em ");
      Serial.print(agora);
      Serial.println(" ms");
    }

    ultimaLeituraPresencaMs = agora;
    estadoAnteriorPresenca = leitura;
  }

  bool presencaAtiva = (agora - ultimoMovimentoMs) < janelaPresencaMs;

  Serial.print("[PIR][TESTE] Pino: ");
  Serial.print(leitura ? "HIGH" : "LOW");
  Serial.print(" | Presenca considerada: ");
  Serial.print(presencaAtiva ? "SIM" : "NAO");
  Serial.print(" | Eventos: ");
  Serial.print(qtdEventosMovimento);
  Serial.print(" | Ultimo movimento ha ");
  Serial.print(agora - ultimoMovimentoMs);
  Serial.println(" ms");

  // Logica antiga mantida comentada para comparacao durante os testes.
  // Serial.print("[SENSOR] Presenca: ");
  // Serial.println(leitura ? "SIM" : "NAO");
  Serial.print("[SENSOR] Temperatura: ");
  if (isnan(temperature)) {
    Serial.println("ERRO na leitura");
  } else {
    Serial.println(temperature);
  }

  // if(leitura) {
  //   qtdDetectouPresenca++;
  // }


  if(SEND_DATA) {
    Serial.println("[INO]: data solicted ");

    monitoringRecord.temperature = temperature;
    monitoringRecord.hasPresent = presencaAtiva ? "S" : "N";

    Serial.print("[SENSOR] Enviando temperatura: ");
    Serial.println(monitoringRecord.temperature);
    Serial.print("[SENSOR] Enviando presenca: ");
    Serial.println(monitoringRecord.hasPresent);

    // Logica antiga mantida comentada para comparacao durante os testes.
    // monitoringRecord.hasPresent = qtdDetectouPresenca > 0 ? "S" : "N";

    controller.sendDataOfMonitoring(monitoringRecord);

    // sendDataToServer(hardware.uuid)

    SEND_DATA = false;

    qtdDetectouPresenca = 0;
    qtdEventosMovimento = 0;
  }
  
  delay(3000);
}
