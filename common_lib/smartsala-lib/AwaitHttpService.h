#ifndef AwaitHttpService_h
#define AwaitHttpService_h

#include <WiFi.h>

#include "BLEServerService.h"
#include "Config.h"
#include "EnvironmentVariablesService.h"
#include "Global.h"
#include "HTTPService.h"
#include "Structs.h"
#include "UtilsService.h"
#include "WiFiService.h"



// #define CONDICIONADOR  "AR_CONDICIONADO"
// #define LUZES "LUZES"
// #define MONITORAMENTO "MONITORAMENTO"
// #define ATUALIZAR "ATUALIZAR_HORARIOS"

class AwaitHttpService 
{
  private: 
    static void awaitsReturn();
    static String getMessageToSend(Solicitacao request);
    static void processConditionerSolicitation(Solicitacao request, String code, bool acting);
    static void processLightsSolicitation(Solicitacao request, bool acting);
    static MonitoringRequest deserealizeObject(String payload);

  public: 
    AwaitHttpService();

    static void awaitSolicitation(void* _this);
    static void executeSolicitation(Solicitacao solicitacao); 

    // Solicitacao
    void startAwait();
    static bool connectToActuator(String uuidDevice); 

};

#endif
