#include "tickdb/common.h"

#include "validate_qql_request.h"


using namespace std;
using namespace tinyxml2;
using namespace DxApiImpl;


ValidateQqlRequest::ValidateQqlRequest(TickDbImpl &db, const std::string &qql) : XmlRequest(db, "validateQQL", false)
{ 
    add("qql", qql);
}


using namespace XmlParse;


template<> bool XmlParse::parse(QqlTokenType &type, const char * from)
{
    
    return NULL == from ? false : (type = QqlTokenType(from), true);
}


template<> bool XmlParse::parse(QqlState::Range &range, const char * from)
{
    return NULL == from ? false : parse(range.value, from);
}


static bool parse(QqlState::Token &token, XMLElement * element)
{
    return parse(token.type, getText(element, "type")) && parse(token.location, getText(element, "location"));
}


bool ValidateQqlRequest::getState(QqlState &state)
{
    state.tokens.reset();
    state.errorLocation.value = -1;

    if (!executeAndParseResponse()) {
        return false;
    }

    XMLElement * rootElement = response_.root();

    if (!nameEquals(rootElement, "validateQQLResponse")) {
        return false;
    }

    XMLElement * resultElement = rootElement->FirstChildElement("result");

    if (NULL == resultElement) {
        DBGLOGERR((string*)NULL, "ERR: 'result' element not found!");
        return false;
    }


    XMLElement * tokensElement = resultElement->FirstChildElement("tokens");

    /*if (NULL == tokensElement) {
        DBGLOGERR((string*)NULL, "ERR: 'tokens' element not found!");
        return false;
    }*/

    auto errorLocationText = getText(resultElement, "errorLocation");
    state.errorLocation.value = -1;
    if (NULL != errorLocationText && !parse(state.errorLocation, errorLocationText)) {
        DBGLOGERR((string*)NULL, "ERR: Unable to parse 'errorLocation' field!");
        return false;
    }

    if (NULL != tokensElement) {
        std::vector<QqlState::Token> tokens;
        for (XMLElement * item = (XMLElement *)tokensElement; NULL != item; item = item->NextSiblingElement("tokens")) {
            QqlState::Token token;
            if (!parse(token, item)) {
                DBGLOGERR((string*)NULL, "ERR: Unable to parse qql state token!");
                return false;
            }

            tokens.push_back(token);
            //tokens.push_back(QqlState::Token());
            /*if (!parse(tokens[tokens.size() - 1], getText(item))) {
                DBGLOGERR((string*)NULL, "ERR: Unable to parse qql state token!");
                return false;
                }*/
        }

        state.tokens = tokens;
    }

    return true;
}


    
