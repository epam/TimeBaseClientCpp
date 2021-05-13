#pragma once
#include "xml_request.h"


namespace DxApiImpl {

    class LoadStreamsRequest : public XmlRequest {
    public:

        typedef std::vector<StreamInfo> Response;

        LoadStreamsRequest(TickDbImpl &db);

        bool getStreams(Response &response);

        bool getStreams(Response &response, const std::string &from);

    protected:
        //bool parseStreamOptions(DxApi::StreamOptions &opt, tinyxml2::XMLElement * e);
    };
}