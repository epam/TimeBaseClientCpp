#pragma once

#include "tickdb/common.h"
#include <dxapi/dxapi.h>

#include "xml_request.h"

namespace DxApiImpl {

    struct TypeTransmissionEnum {
        enum Enum {
            GUID,
            NAME,
            DEFINITION
        };
    };

    ENUM_CLASS(uint8_t, TypeTransmission);


    class SelectionOptionsImpl : public DxApi::SelectionOptions {
    public:
        TypeTransmission    typeTransmissionMode;
        DxApi::Nullable<std::string> qql;
        std::vector<DxApi::QueryParameter> qqlParameters;

    public:
        SelectionOptionsImpl() : SelectionOptions(),
            typeTransmissionMode(TypeTransmission::DEFINITION)
        {}

        SelectionOptionsImpl(const SelectionOptions &other) : SelectionOptions(other),
            typeTransmissionMode(TypeTransmission::DEFINITION)
        {}

        SelectionOptionsImpl(const SelectionOptionsImpl &other) : SelectionOptions(other),
            typeTransmissionMode(TypeTransmission::DEFINITION), qql(other.qql),
            qqlParameters(other.qqlParameters)
        {}
    };

    template<> DxApiImpl::XmlGen::XmlOutput& operator<<(DxApiImpl::XmlGen::XmlOutput& xml, const SelectionOptionsImpl &v);
}