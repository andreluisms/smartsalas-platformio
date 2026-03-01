#ifndef EquipmentService_h
#define EquipmentService_h

// using namespace std;

#ifdef _WIN32
    #include <Vector>
#else
    #include <vector>
#endif


#include <Arduino.h>
#include <EmonLib.h>
#include "Global.h"
#include "Config.h"

#define RELE 19
#define pinSCT 34 
#define tensao 127
#define MAX_SIZE_COMMAND 400

class EquipmentService 
{
  public: 
    
    bool __lightOn;
    EnergyMonitor __sct13;
    
    EquipmentService();

    EnergyMonitor geSct();
    void setSct(EnergyMonitor light);

    bool getLightOn();
    void setLightOn(bool light);
    void SplitIrComands(String data, vector<int> &codigo);
    String SplitGetIndex(String data, char separator, int index);
    void SendIrComand(std::vector <int> codigo);
    bool checkIrms();
    void checkOperationLights(String msg);
    void turnOnLights();
    void turnOffLights();
    String executeActionFromController(String data);
    String executeActionIntoConditioner(String command, String state);
};

#endif
