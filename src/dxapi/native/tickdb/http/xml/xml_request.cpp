#include "tickdb/common.h"

#include <dxapi/dxapi.h>
#include "xml_request.h"


using namespace std;
using namespace tinyxml2;
using namespace DxApi;
using namespace DxApiImpl;


static INLINE const char * get_text(XMLElement * const element)
{
    return NULL != element ? element->GetText() : NULL;
}

namespace DxApiImpl {
    namespace XmlGen {

        //std::string itemTag(const std::string &content)
        //{
        //    return tag("item", content);
        //}


        //std::string streamTag(const std::string &content)
        //{
        //    return tag("stream", content);
        //}


        //std::string instrumentsTagContent(const InstrumentIdentity entities[], size_t numEntities)
        //{

        //    string s;
        //    if (NULL != entities) {
        //        forn(i, numEntities) {
        //            const InstrumentIdentity &ent = entities[i];
        //            s.append("\n").append("<item>")
        //                .append(tag("instrumentType", ent.type))
        //                .append(tag("symbol", ent.symbol))
        //                .append("</item>");
        //        }
        //    }
        //    return s.append("\n"); // TODO: Do something about automatic CRLF conversion
        //}


        /*std::string instrumentTypesList(const std::vector<DxApi::InstrumentType> * instrTypes)
        {
            string s;
            if (NULL != instrTypes) {
                for (auto i : *instrTypes) {
                    s.append(XmlGen::tag("instrumentType", i));
                }
            }
            return s;
        }*/


        /*void escape(std::string& dest, const std::string& src)
        {
            dest.clear();
            dest.reserve(src.size());
            for (char c : src) {
                switch (c) {
                case '&':  dest.append("&amp;");       break;
                case '\"': dest.append("&quot;");      break;
                case '\'': dest.append("&apos;");      break;
                case '<':  dest.append("&lt;");        break;
                case '>':  dest.append("&gt;");        break;
                default:   dest.push_back(c); break;
                }
            }
        }*/
    }
}

namespace DxApiImpl {
    namespace XmlParse {

        const char * getText(XMLElement * const element)
        {
            return get_text(element);
        }


        const char * getText(XMLElement * const element, const char * childName)
        {
            return NULL != element ? get_text(element->FirstChildElement(childName)) : NULL;

        }


        bool nameEquals(XMLElement * const element, const char * elementName)
        {
            return NULL != element && 0 == strcmp(element->Name(), elementName);
        }
        

        template<> bool parse(string &to, const char * source)
        {
            if(NULL != source) {
                to = source;
                return true;
            }

            return false;
        }


        template<> bool parse(Nullable<string> &to, const char * source)
        {
            if (NULL != source) {
                to = source;
            }
            else {
                to.reset();
            }

            return true;
        }

        
        template<> bool parse(int64 &to, const char * source)
        {
            if(NULL != source) {
                char * endptr;
                to = strtoll(source, &endptr, 10);
                return NULL != endptr && source != endptr;
            }

            return false;
        }

        
        template<> bool parse(int32 &to, const char * source)
        {
            if(NULL != source) {
                char * endptr;
                to = (int)strtol(source, &endptr, 10);
                return NULL != endptr && source != endptr;
            }

            return false;
        }
        
        template<> bool parse(uint64 &to, const char * source)
        {
            if(NULL != source) {
                char * endptr;
                to = strtoull(source, &endptr, 10);
                return NULL != endptr && source != endptr;
            }

            return false;
        }
        
        template<> bool parse(uint32 &to, const char * source)
        {
            if(NULL != source) {
                char * endptr;
                to = strtoul(source, &endptr, 10);
                return NULL != endptr && source != endptr;
            }

            return false;
        }

        template<> bool parse(double &to, const char * source)
        {
            if(NULL != source) {
                char * endptr;
                to = strtod(source, &endptr);
                return NULL != endptr && source != endptr;
            }

            return false;
        }
        
        template<> bool parse<bool>(bool &to, const char * source)
        {
            if(NULL != source) {
                if (0 == _strcmpi(source, "true")) {
                    to = true;
                    return true;
                }
                else if (0 == _strcmpi(source, "false")) {
                    to = false;
                    return true;
                }
            }
            
            return false;
        }
        

        template<> bool parse(StreamScope &to, const char * source)
        {
            if (NULL != source) {
                to = StreamScope::fromString(source);
                return true;
            }
            return false;
        }


        void unescape(string& dest, const string& src) 
        {
            dest.clear();
            dest.reserve(src.size());
            const char * r = src.c_str();
            char c;
            while ('\0' != (c = *r++)) {
                if ('&' == c) {
                    if (0 == strncmp(r, "amp;", 4)) {
                        r += 4;
                        // c == '&'
                    }
                    else if (0 == strncmp(r, "quot;", 5)) {
                        r += 5;
                        c = '\"';
                    }
                    else if (0 == strncmp(r, "apos;", 5)) {
                        r += 5;
                        c = '\'';
                    }
                    else if (0 == strncmp(r, "lt;", 3)) {
                        r += 3;
                        c = '<';
                    }
                    else if (0 == strncmp(r, "gt;", 3)) {
                        r += 3;
                        c = '>';
                    }
                    else {
                        // Do nothing
                    }
                }

                dest.push_back(c);
            }
        }
    }
}

XmlRequest::XmlRequest(TickDbImpl &db, const char * requestName, bool noContent) : noException(false), allowUnopenedDb(false), db_(db), name_(requestName), noContent_(noContent), noRequestNeeded_(false)
{
    begin(&request_, requestName, noContent);
}


string XmlRequest::end()
{
    request_.closeTag(name_);
    return request_.str();
}


XmlInput::~XmlInput()
{
    if (NULL != xmlDocument_) {
        delete xmlDocument_;
    }
}


XmlInput::XmlInput() : xmlDocument_(NULL), error_(0)
{

}


XMLError XmlInput::parseError() const
{
    return (XMLError)error_;
}

string XmlInput::parseErrorText() const
{
    string t;
    if (parseError()) {
        t.append(toString(parseError()));
        if (NULL != xmlDocument_) {
            const char * s1 = xmlDocument_->GetErrorStr1();
            const char * s2 = xmlDocument_->GetErrorStr2();
            if (NULL != s1) {
                t.append(" ").append(s1);
            }

            if (NULL != s2) {
                t.append(" ").append(s2);
            }
        }
    }

    return t;
}


bool XmlInput::parse()
{
    if (NULL == xmlDocument_) {
        xmlDocument_ = new tinyxml2::XMLDocument();
    }
    else {
        xmlDocument_->Clear();
    }

    XMLError error = xmlDocument_->Parse(this->data(), this->size());
    error_ = error;
    if (XML_NO_ERROR == error) {
        return true;
    }
    else {
        delete xmlDocument_;
        xmlDocument_ = NULL;
        return false;
    }
}


XMLElement * XmlInput::root()
{
    // If already parsed with error, return NULL
    if (XML_NO_ERROR != error_) {
        return NULL;
    }

    // If already parsed or parsing is succesful, return root
    if (NULL != xmlDocument_ || parse()) {
        return xmlDocument_->RootElement();
    }

    // parsing failed, return NULL;
    return NULL;
}


// Default implementation just returns request string, assuming it is completely formed
string XmlRequest::getRequest()
{
    return noContent_ ? request_.str() : end();
}


XmlRequest::~XmlRequest()
{
}


static void dbg_log_xml(const char *name, const char *s)
{
    FILE *f = NULL;
    static bool opened = false;
    // TODO: Switch to decent logger
    forn(i, 5) {
        f = fopen(dbg_logfile_path("xml_req", "xml").c_str(), opened ? "a+t" : "wt");
        if (NULL != f) {
            break;
        }
        spin_wait_ms(15);
    }

    opened = true;

    if (NULL != f) {
        fprintf(f, NULL == s || 0 == strlen(s) ? "<!-- %s - empty response -->\n" : "<!-- %s -->\n%s\n", name, s);
        fclose(f);
    }
}


bool XmlRequest::executeWithTextResponse(string &response)
{
    std::string request = getRequest();

    if (noRequestNeeded_) {
        return true;
    }

#if defined(_DEBUG) && DBG_LOG_ENABLED == 1 && XML_LOG_LEVEL >= 2
    dbg_log_xml(std::string(name_).c_str(), request.c_str());
#endif
    //DBGLOG("db = " FMT_HEX64, &db_);
    //throw XmlErrorResponseException(httpResponseCode, responseBody);sdas das

    int httpResult = db_.executeTbPostRequest(response, request, name_, allowUnopenedDb);
    if (!HttpSuccess(httpResult)) {
        string msg("Unable to execute XML HTTP request: ");

        msg.append(response);
        string unescaped;
        XmlParse::unescape(unescaped, response);
        response = move(unescaped);

        if (noException) {
            // TODO: Shouldn't log this always?
            DBGLOGERR((std::string*)NULL, "%s", msg.c_str());
            return false;
        }

#if DBG_LOG_ENABLED == 1 && XML_LOG_LEVEL >= 1
        dbg_log_xml("ERROR !!!!", " ");
        dbg_log_xml(string(name_).append(" request").c_str(), request.c_str());
        dbg_log_xml(string(name_).append(" response(error)").c_str(), response.c_str());
#endif
        DBGLOGERR((std::string*)NULL, "%s", msg.c_str());
        throw XmlErrorResponseException(httpResult, response);
    }

#if defined(_DEBUG) && DBG_LOG_ENABLED == 1 && XML_LOG_LEVEL >= 2
    dbg_log_xml(string(name_).append(" response").c_str(), response.c_str());
#endif
    return true;
}


bool XmlRequest::executeWithTextResponse()
{
    return executeWithTextResponse(response_);
}

void XmlRequest::responseParseError(const char * err)
{
#if DBG_LOG_ENABLED == 1 && XML_LOG_LEVEL >= 1
    dbg_log_xml((string(name_).append(" response parsing failed: ").append(err).append("\n")).c_str(), response_.c_str());
#endif
}


bool XmlRequest::executeAndParseResponse()
{

    if (!executeWithTextResponse()) {
        return false;
    }

    if (response_.parse()) {
        return true;
    }

    string s = response_.parseErrorText();
    responseParseError(s.c_str());
    THROW_DBGLOG("%s", s.c_str());

    return false;
}




#if 0
template<> XmlOutput&  XmlOutput::add(const char * name, const std::string strings[], size_t numStrings)
{
    std::string s;
    if (NULL != strings) {
        openTag(tagName);
        forn(i, numStrings) {
            openTag("item").add(strings[i]).closeTag("item");
        }
        closeTag(tagName).add('\n');
    }
    else {
        // write nothing, if NULL
    }
}



template<> XmlOutput&  XmlOutput::add(const char * name, const DxApi::TickStream * streams[], size_t numStreams)
{
    std::string s;
    if (NULL != streams) {
        forn(i, numStreams) {
            s.append(itemTag(streams[i]->key()));
        }
        req_ << tag(name, s) << '\n';
    }
    else {
        // write nothing, if NULL
    }
}
#endif

using namespace XmlGen;

XmlOutput::XmlOutput() {}

// NOTE: Move constructor disabled due to stringstream having no copy/move/swap in some C++ compilers
//XmlOutput::XmlOutput(XmlOutput &&other) : std::stringstream(std::move(static_cast<stringstream&>(other))) { }
//XmlOutput::XmlOutput(XmlOutput &&other)
//{
    //std::swap(static_cast<stringstream&>(*this), static_cast<stringstream&>(other));
//}

XmlOutput& XmlOutput::openTag(const char * tagName)
{
    return (*this).put('<').put(tagName).put('>');
}


XmlOutput& XmlOutput::closeTag(const char * tagName)
{
    return (*this).put("</").put(tagName).put('>');
}


namespace DxApiImpl {
    XmlOutput& operator<<(XmlOutput &to, const TickStream *stream)
    {
        return to.add(stream->key());
    }


    //template<> XmlOutput& operator<<(XmlOutput &to, const InstrumentIdentity &instrument)
    //{
    //    return to.add("instrumentType", instrument.type.toString()).add("symbol", instrument.symbol);
    //}

    XmlOutput& XmlOutput::addInstrumentTypeArray(const std::vector<DxApi::InstrumentType> * const instrumentTypes)
    {
        if (NULL != instrumentTypes) {
            for (auto i : *instrumentTypes) {
                add("instrumentTypes", i);
            }
        }

        return *this;
    }
}