#include <algorithm>    // find
#include <cctype>
#include <cassert>

#include "uri.h"


bool Uri::parse(const char * uri)
{
    return NULL == uri ? false : parse(std::string(uri));
}


#if 1
bool Uri::parse(const std::string &uri)
{
    typedef string::const_iterator iterator_t;

    if (uri.length() == 0) {
        return false;
    }

    full = uri;
    queryString.clear();
    path.clear();
    protocol.clear();
    host.clear();
    portString.clear();
    port = 0;

    iterator_t uriEnd = uri.end();

    // get query start
    iterator_t queryStart = find(uri.begin(), uriEnd, '?');

    // query
    if (queryStart != uriEnd) {
        queryString = string(queryStart, uri.end());            // +queryString
    }

    // protocol
    iterator_t protocolStart = uri.begin();
    iterator_t protocolEnd = find(protocolStart, queryStart, ':');            //"://");

    if (protocolEnd != queryStart) {
        string prot = &*(protocolEnd);
        if ((prot.length() > 3) && (prot.substr(0, 3) == "://")) {
            protocol = string(protocolStart, protocolEnd);          // +protocol
            protocolEnd += 3;   //      ://
        }
        else {
            protocolEnd = uri.begin();  // no protocol
        }
    }
    else {
        protocolEnd = uri.begin();  // no protocol
    }

    // host
    iterator_t hostStart = protocolEnd;
    iterator_t pathStart = find(hostStart, queryStart, '/');  // get pathStart

    // path
    if (pathStart != uriEnd) {
        path = string(pathStart, queryStart);       // Path
    }

    // Remainder is [hostStart, pathStart)


    iterator_t hostEnd = find(hostStart, pathStart, ':');  // check for port

    host = string(hostStart, hostEnd);      // + host

    // port
    if (hostEnd != pathStart) {
        assert(':' == *hostEnd);
        // we have a port
        portString = string(hostEnd + 1, pathStart);
    }

    // Limited verification

    // Verify and parse port
    char *end;
    unsigned long long port_ull = 0;

    if (0 != portString.size()) {
        port_ull = strtoull(portString.c_str(), &end, 10);
        if (end - portString.c_str() != portString.size())
            return false;
    }

    port = port_ull & 0xFFFF;
    if (port != port_ull)
        return false;

    return true;
}


#else
bool Uri::parse(const std::string &uri)
{
    typedef string::const_iterator iterator_t;

    if (uri.length() == 0) {
        return false;
    }

    full = uri;
    queryString.clear();
    path.clear();
    protocol.clear();
    host.clear();
    port.clear();

    size_t length = uri.size();
    char * s = (char *)alloca(length + 8);
    if (NULL == s) {
        return false;
    }
    memcpy(s, uri.c_str(), length);
    s[length] = '\0';

    const char *r = s;

    while (*r && ':' != *r) ++r;
    if (!*r) {
        // No protocol
        r = s;
    }
    else {

    }

}

#endif