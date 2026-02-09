#ifndef HTTP_h
#define HTTP_h

#include "Config.h"

class HTTP
{
    public:
        HTTP();
        String request(String, String, String) const;
};

extern HTTP http;

#endif
