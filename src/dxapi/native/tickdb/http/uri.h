#pragma once

#ifndef CPPAPI_DELTIX_CPP_URI_H
#define	CPPAPI_DELTIX_CPP_URI_H

#include <string>

struct Uri {
    typedef std::string string;

public:
    string full, queryString, path, protocol, host, portString;
    unsigned port;

    bool parse(const char * uri);
    bool parse(const std::string &uri);

};

#endif