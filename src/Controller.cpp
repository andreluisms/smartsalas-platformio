#include "Config.h"
#include "Controller.h"
#include "BLESensorService.h"

// Construtor da classe Controller
Controller::Controller(){}

// Instâncias globais de serviços utilizados pelo Controller
HTTPService __httpservice;                                 // Responsável por comunicação HTTP
AwaitHttpService __awaitHttpService;                // Serviço de espera/controle de requisições HTTP
BLEServerService* __bleConfig;                      // Ponteiro para o serviço BLE (deve ser inicializado em outro lugar)
EnvironmentVariablesService __environmentService;   // Gerencia variáveis de ambiente
EquipmentService __equipmentService;                // Executa ações em equipamentos
UtilsService __utilService;                         // Funções utilitárias


// Busca informações do hardware no servidor
// Retorna true se o hardware for encontrado e válido
bool Controller::start(HardwareRecord &record) const {   
    // Cria uma estrutura local de hardware inicializada com valores vazios
    struct HardwareRecord hardware = {0,"",""};

    // Busca informações do hardware via HTTP
    __httpservice.getInfoHardware(hardware);

    // Se o hardware retornado tem ID válido, 
    // copia os dados para o parâmetro de saída
    if(hardware.id > 0 ){ 
        record = hardware;
        return true;
    }else
        return false;
}



// Reinicia o dispositivo
void Controller::restart() const {
  Hardware hardware;
  hardware.restart();
  return ;
}


// Registra o hardware no servidor
// Retorna true se o registro for bem-sucedido
bool Controller::registerHardware(HardwareRecord hardware) const{   
    bool statusRegister = false;

    // Envia os dados do hardware para o servidor
    statusRegister = __httpservice.registerHardware(hardware);
    if(statusRegister)
        return true;
    return false;
}
        

// Método reservado para notificar o servidor (atualmente não implementado)
bool Controller::notificateServer() const{
    return true;
}


// Inicia a task do servidor BLE após atraso de 2 segundos
void Controller::startBLETaskServer(){
    delay(2000);
    __bleConfig->startTaskBLE();
}


// Configura o servidor BLE
void Controller::setupBLEServer(){
    __bleConfig->initBLE();          // Inicializa o BLE
    __bleConfig->activeBLEScan();    // Ativa o modo de scan
    __bleConfig->scanDevices();      // Realiza o scan de dispositivos
    __bleConfig->stopScan();         // Para o scan
    __bleConfig->populateMap();      // Organiza os dispositivos encontrados
}


// Configura o cliente BLE para um dispositivo específico
void Controller::setupBLEClient(String deviceName, DeviceType deviceType){
   initBLEClient(deviceName, deviceType);  
}


// Obtém sensores associados ao hardware (atualmente desativado)
void Controller::getSensors(HardwareRecord hardware, String sensors[], int &indexSensor){
    //__httpservice.getSensors(hardware, sensors, indexSensor);
    return;
}


// Obtém o master associado ao hardware
// Retorna true se um master válido for encontrado
bool Controller::getMaster(HardwareRecord hardware, String &master){
    __httpservice.getMaster(hardware, master);
    return !master.equals("") ? true : false;
}

 

// Envia dados de monitoramento ao servidor
void Controller::sendDataOfMonitoring(MonitoringRecord monitoringRecord){
    // Monta a string com os dados de monitoramento
    String data = __utilService.mountDataMonitoring(monitoringRecord);
     // Envia os dados para o servidor
    sendDataToServer(data);
}



// Inicia a task responsável por aguardar respostas HTTP
void Controller::startTaskHttp(){  
    __awaitHttpService.startAwait();
}


// Retorna a configuração de hardware armazenada nas variáveis de ambiente
HardwareRecord Controller::getHardwareConfig(){
    return __environmentService.getHardware();
}


// Atualiza a configuração de hardware nas variáveis de ambiente
void Controller::setHardwareConfig(HardwareRecord hardware){
    __environmentService.setHardware(hardware);
}


// Validação contínua das variáveis de ambiente
void Controller::environmentVariablesContinuousValidation(){
    __environmentService.continuousValidation();
}

// Inicializa as variáveis de ambiente
void Controller::setupEnvironmentVariables(){
    __environmentService.initEnvironmentVariables();
}


// Preenche a lista de sensores e atuadores obtidos do servidor
void Controller::fillHardwares(HardwareRecord hardware){
     // Obtém a lista de hardwares associados
    std::vector<struct HardwareRecord> hardwares = __httpservice.getHardwares(hardware);
    
     // Percorre cada hardware retornado
    for(struct HardwareRecord r : hardwares){
        // Se for sensor, adiciona como sensor BLE
        if(r.typeHardwareId == TYPE_SENSOR)
            __bleConfig->addSensor(r.uuid);
        else
        // Caso contrário, adiciona como atuador
            __bleConfig->addActuator(r);
    } 
}


// Verifica se existem sensores ou atuadores carregados
bool Controller::loadedDevices(){
    if ( __bleConfig->getActuators().size() > 0 || __bleConfig->getSensors().size() > 0)
        return true;

    return false;
}


// Executa um comando recebido e envia a resposta ao servidor
void Controller::ExecuteCommand(String command) {
  // Executa a ação no equipamento
  String response = __equipmentService.executeActionFromController(command);
  // Envia a resposta ao servidor
  sendDataToServer(response);
}