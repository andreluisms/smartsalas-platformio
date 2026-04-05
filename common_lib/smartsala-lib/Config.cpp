#include "Config.h"
#include "UtilsService.h"

String __url;
String __ssid;
String __password;
int    __ledStatus;
int    __basetime;
int    __type;
int    __route;
bool   __debug;
Config config;
std::mutex Config::__bleActuatorMutex;
std::mutex Config::__envVariablesMutex;

Config::Config(){
    __tokenApp  = "594ac3eb82b5080393ad5c426f61c1ed5ac53f90e1abebc15316888cf1c8f5fe";
    __url       = "http://itetech-001-site4.qtempurl.com/api";
    __debug     = true;
    __ssid      = "U.W.N";
    __password  = "1011122025";
    __basetime  = 180000;
    __ledStatus = 2;
    __type  = TYPE_CONTROLLER;
    __route = 2;
    __wifiFailAttempts = 5;
    __commandSendAttempts = 3;
    __timesToHasOne = 3;
    // quando false, solicita o ssid e password ao usuário via serial monitor
    __defaultssid = false;
    
    pinMode(__ledStatus, OUTPUT);
}

String Config::getTokenApp()
{ 
	return __tokenApp;
}

String Config::getUrl()
{ 
	return __url;
}

bool Config::isDefaultssid(){
  return __defaultssid;
}

String Config::getSSID()
{
	return __ssid;
}

void Config::setSSID(String ssid)
{
	__ssid = ssid;
}

String Config::getPassword()
{
	return __password;
}

void Config::setPassword(String password)
{
	__password = password;
}

int	Config::getBaseTime()
{
	return __basetime;
}

int	Config::getType()
{
	return __type;
}

int	Config::getLedStatus() 
{
	return __ledStatus;
}

int Config::getRoute()
{
    return __route;
}

int Config::getWifiFailAttempts()
{
    return __wifiFailAttempts;
}

int Config::getCommandSendAttempts()
{
    return __commandSendAttempts;
}

bool Config::isDebug() 
{
	return __debug;
}

int Config::getTimesToHasOne()
{
  return __timesToHasOne;
}

void Config::lock() 
{
	__bleActuatorMutex.lock();
}

void Config::unlock()
{
  __bleActuatorMutex.unlock();
}

void Config::lockEnvVariablesMutex() 
{
	__envVariablesMutex.lock();
}

void Config::unlockEnvVariablesMutex()
{
  __envVariablesMutex.unlock();
}

