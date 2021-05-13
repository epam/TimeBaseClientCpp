#pragma once

#include "tickdb/common.h"
#include <dxapi/dxapi.h>
#include "../xml_request.h"


namespace DxApiImpl {

    class BackgroundProcessInfoImpl : public DxApi::BackgroundProcessInfo {
    public:
        BackgroundProcessInfoImpl() : BackgroundProcessInfo()
        { };

    public:
        std::string toXML() const;

        bool fromXML(tinyxml2::XMLElement * root);
    };

    template<> XmlGen::XmlOutput& operator<<(XmlGen::XmlOutput& xml, const BackgroundProcessInfoImpl &value);

    DEFINE_IMPL_CLASS(DxApi::BackgroundProcessInfo, BackgroundProcessInfoImpl)
}