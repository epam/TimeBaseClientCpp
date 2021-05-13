#include "tickdb/common.h"

#include "lock_stream_request.h"


using namespace std;
using namespace tinyxml2;
using namespace DxApiImpl;


// This class is probably redundant. Created due to wrong assumptions that both requests return similar XML response. Or maybe will do that later

INLINE LockUnlockRequest::LockUnlockRequest(const DxApi::TickStream * stream, const char *name, bool write)
    : StreamRequest(stream, name), write_(write)
{
    add("write", write);
}


LockStreamRequest::LockStreamRequest(const DxApi::TickStream * stream, const std::string &sid, bool write, int64_t timeout)
    : LockUnlockRequest(stream, "lockStream", write)
{
    add("timeout", timeout);
    add("sid", sid);
}


UnlockStreamRequest::UnlockStreamRequest(const DxApi::TickStream * stream, const std::string &id, bool write)
    : LockUnlockRequest(stream, "unlockStream", write)
{
    add("id", id);
    id_ = id;
}


using namespace XmlParse;

bool LockStreamRequest::execute()
{
    if (!executeAndParseResponse())
        return false;

    XMLElement * rootElement = response_.root();

    if (!nameEquals(rootElement, "lockStreamsResponse")) {
        return false;
    }

    const char * s = getText(rootElement, "id");
    if (NULL == s) {
        return false;
    }

    id_ = s;

    if (!parse(write_, getText(rootElement, "write"))) {
        return false;
    }

    return true;
}


bool UnlockStreamRequest::execute()
{
    return executeWithTextResponse();
}
