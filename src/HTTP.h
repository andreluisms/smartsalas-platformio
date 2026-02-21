#ifndef HTTP_h
#define HTTP_h

#include <Arduino.h>
#include <HTTPClient.h>
#include "Config.h"
#include "WiFi.h"

class HTTP
{
    public:
        HTTP();
        String request(String, String, String) const;
};

extern HTTP http;

#endif
