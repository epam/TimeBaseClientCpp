#pragma once

#include "tickdb/common.h"
#include <dxapi/dxapi.h>
#include "xml_request.h"


namespace DxApiImpl {

    class BufferOptionsImpl : public DxApi::BufferOptions {
    public:
        BufferOptionsImpl() : BufferOptions()
        { };

    public:
        std::string toXML() const;
        
        // Should be called on new streamoptions object! Return true on success.
        bool fromXML(tinyxml2::XMLElement * root);
    };

    DEFINE_IMPL_CLASS(DxApi::BufferOptions, BufferOptionsImpl)

    class StreamOptionsImpl : public DxApi::StreamOptions {
    public:
        StreamOptionsImpl() : StreamOptions()
        {}
        

    public:        
        // Should be called on new streamoptions object! Return true on success.
        bool fromXML(tinyxml2::XMLElement * root);
    };
    

    template<> XmlGen::XmlOutput& operator<<(XmlGen::XmlOutput& xml, const StreamOptionsImpl &value);

    DEFINE_IMPL_CLASS(DxApi::StreamOptions, StreamOptionsImpl)
}