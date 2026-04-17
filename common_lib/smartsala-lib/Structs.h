#ifndef BLESensorSettings_h
#define BLESensorSettings_h

#include <NimBLEDevice.h>
// #include "BLEDevice.h"

// Hardware
struct HardwareRecord
{
    int id;
    String token;
    String uuid; 
    int salaId;
    int typeHardwareId;
    int typeEquipment;  
};

// Hardware
struct MonitoringRecord
{
    String hasPresent;
    float temperature; 
};

struct BLEDeviceConnect
{
  BLEClient* pClient;
  BLERemoteCharacteristic* pRemoteCharacteristic;
  BLERemoteCharacteristic* pRemoteHardwareCharacteristic;
  BLERemoteService* pRemoteService;
  String uuid = "";
  bool deviceFound = true;
};

struct MonitoringRequest
{
  String type;
  String code;
  String uuid;
  String acting;
};

/*
 * Estrutura usada para guardar dados da solicitacao
 */
struct Solicitacao
{
  int id;
  String type;
  String code;
  String uuid;
  String acting;
};


/*
 * Estrutura usada para guardar dados da reserva da sala
 */
struct Reserva {
  int id;
  String  date;
  String  horarioInicio;
  String  horarioFim;
  String  situacao;
  String  objetivo;
  int usuarioId;
  int salaId;
  int planejamento;
};

/*
 * Estrutura usada para guardar dados do monitoramento da sala
 */
struct Monitoramento {
  int id;
  bool estado;
  int equipamentoId;
};


#endif
