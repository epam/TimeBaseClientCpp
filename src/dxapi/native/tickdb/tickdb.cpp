#include "tickdb/common.h"

#include "http/tickdb_http.h"


using namespace std;
using namespace DxApi;
using namespace DxApiImpl;


TickDb * TickDb::createFromUrl(const char * url, const char * username, const char * password)
{
    TickDbImpl *db = new TickDbImpl();

    // Does not check if the uri is connectable at this moment
    if (!db->setUri(url)) {
        delete db;
        THROW_DBGLOG_EX(TickDbException, "Unable to parse URI: %s\n", url);
        return NULL;
    }

    if (NULL != username) {
        if (!db->setAuth(username, password)) {
            delete db;
            THROW_DBGLOG_EX(TickDbException, "Unable to set authentication information for URI: %s\n", url);
            return NULL;
        }
    }

    DBGLOG("%s: Will connect to: %s\n", db->textId(), url);

    return db;
}

TickDb * TickDb::createFromUrl(const char * url)
{
    return createFromUrl(url, NULL, NULL);
}


TickDb::~TickDb()
{
    // Nothing here for now
}


std::string MarketMessage::toString() const {
    std::stringstream ss;
    //TickCursor * cursor = reinterpret_cast<TickCursor*>(parent);
    ss << "<" << *cursor->getInstrument(entityId) << "|" << typeId << ">";
    return ss.str();
}


const std::string * InstrumentMessage::getTypeName() const {
    return cursor->getMessageTypeName(typeId);
}

