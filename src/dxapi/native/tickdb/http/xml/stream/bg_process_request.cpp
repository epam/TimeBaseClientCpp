#include "tickdb/common.h"

#include "bg_process_request.h"
#include "bg_proc_info_impl.h"


using namespace std;
using namespace tinyxml2;
using namespace DxApiImpl;
using namespace XmlParse;


bool GetBgProcessRequest::getResponse(DxApi::BackgroundProcessInfo *bgProcessInfo)
{
    if (NULL == bgProcessInfo || !executeAndParseResponse())
        return false;

    XMLElement *rootElement = response_.root();

    if (!nameEquals(rootElement, "BgProcess"))
        return false;

    XMLElement *content = rootElement->FirstChildElement("info");

    if (NULL == content)
        return false;

    return impl(*bgProcessInfo).fromXML(content);
}