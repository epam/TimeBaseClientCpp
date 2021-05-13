#pragma once

#include "tickdb/http/tickdb_http.h"
#include "xml/tinyxml2/tinyxml2.h"


#define FOR_XML_ELEMENTS(parentElement, i, name) for (XMLElement * (i) = (parentElement)->FirstChildElement(name); NULL != (i); (i) = (i)->NextSiblingElement())


namespace DxApiImpl {
    //template<> std::string INLINE toString<DxApi::InstrumentType>(const DxApi::InstrumentType &x) { return x.toString(); }

    class XmlErrorResponseException : public std::runtime_error {
    public:
        int errorCode() const { return errorCode_; }

        XmlErrorResponseException(int errorCode, const std::string &message) : std::runtime_error(message), errorCode_(errorCode)
        {}

    protected:
        int errorCode_;

    };

    static INLINE bool HttpSuccess(int x) { return x / 100 == 2; }


    namespace XmlGen {
        //template <class T> std::string tag(const char * tagName, const T &content);
        //std::string instrumentTypesList(const std::vector<DxApi::InstrumentType> * instrTypes);

        void escape(std::string& dest, const std::string& src);


        class XmlOutput : std::stringstream {
        public:
            std::stringstream& ostream();

            XmlOutput& openTag(const char * tagName);
            XmlOutput& closeTag(const char * tagName);
            std::string str() const { return std::stringstream::str(); }

            XmlOutput& put(char data);
            XmlOutput& put(const char * data, size_t size);
            XmlOutput& put(const char * string);

            XmlOutput& addChar(char c);
            XmlOutput& addChars(const char * src, size_t n);

            template <typename T> XmlOutput& add(const char *name, const T &content);
            XmlOutput& add(const char * name, const char *content); // C strings are allowed
            template <typename T> XmlOutput& add(const char *name, const T *content) = delete; // No pointers, please
            template <typename T> XmlOutput& addItemArray(const char *name, const T values[], size_t count);
            template <typename T> XmlOutput& addItemArray(const char *name, const std::vector<T> *valuesVector);
            template <typename T> XmlOutput& addItemArray(const char *name, const std::vector<T> &valuesVector);
            template <typename P, typename Q> XmlOutput& addItemMap(const char *name,
                const char * keyName, const char * valueName, const std::unordered_map<P, Q> &values);
            template <typename P, typename Q> XmlOutput& addItemMap(const char *name,
                const char * keyName, const char * valueName, const DxApi::Nullable<std::unordered_map<P, Q>> &values);
            
            // TODO: remove
            XmlOutput& addInstrumentTypeArray(const std::vector<DxApi::InstrumentType> * const instrumentTypes);

            XmlOutput& add(const char *value);
            template <typename T> XmlOutput& add(const T &value);

            XmlOutput();
            //XmlOutput(XmlOutput &&other);
        private:
            template <typename T> XmlOutput& addItems(const T values[], size_t count);
        };
    }

    XmlGen::XmlOutput makeRequestHeader(const char * req, bool closeTag = true);

    XmlGen::XmlOutput& operator<<(XmlGen::XmlOutput &to, const DxApi::TickStream * const value);

    template<typename T> XmlGen::XmlOutput& operator<<(XmlGen::XmlOutput &to, const DxApi::Nullable<T> &value);
    XmlGen::XmlOutput& operator<<(XmlGen::XmlOutput &to, const char *value);
    template<typename T> XmlGen::XmlOutput& operator<<(XmlGen::XmlOutput &to, const T &value);

    // No pointers, please (except const char *)
    template<typename T> XmlGen::XmlOutput& operator<<(XmlGen::XmlOutput &to, const T *value) = delete;


    namespace XmlParse {
        const char * getText(tinyxml2::XMLElement * const element);

        const char * getText(tinyxml2::XMLElement * const element, const char * childName);

        bool nameEquals(tinyxml2::XMLElement * const element, const char * elementName);

        template<typename T> bool parse(T &to, const char * source);

        void unescape(std::string& dest, const std::string& src);
    };

#if 0
    // the class is supposed to contain "serialized" tree, whose root is at the end of the tree
    class XmlGenItem {
        friend class XmlGen;
        enum TagType {
            NIL,
            LEAF_EMPTY,
            LEAF_WITH_CONTENT
        };


        void grow(intptr_t numbytes) { buffer_.append(numbytes, '\0'); }


        int32_t get_int32(intptr_t offset)
        {
            assert(offset + 4 <= buffer_.size());
            return _loadLE<int32_t>(&buffer_[offset]);
        }

        void set_int32(int32_t value, char *ptr)
        {
            assert((size_t)(ptr - &buffer_[0]) <= buffer_.size());
            _storeLE<int32_t>(ptr, value);
        }

        void set_int32(int32_t value, intptr_t offset)
        {
            assert(offset + 4 <= buffer_.size());
            _storeLE<int32_t>(&buffer_[offset], value);
        }


        void add_int32(int32_t value)
        {
            intptr_t offset = buffer_.size();
            grow(4);
            set_int32(value, offset);
        }

        XmlGenItem(const char * s)
        {
            size_t len = strlen_ni(s);
            buffer_.append(s, len);
            add_int32(len);
            buffer_.append(1, LEAF_EMPTY);
        }

        // add sub-tag with text s as content
        void enclose(XmlGenItem &parent, const char * s)
        {
            uint8_t * p = (uint8_t *)&buffer_[buffer_.size() - 1];
            assert(LEAF_EMPTY == *p);
            size_t len = strlen_ni(s);
            grow(3);
            set_int32()

            buffer_.append(s, len);
            add_int32(len);
            buffer_.append(1, LEAF);
        }


    protected:
        std::string buffer_;
    };

    class XmlGen {
        tag()
        template <class T> static std::string tag(const char * tagName, const T &content) {
        return string("<").append(tagName).append(">").append(toString(content)).append("</").append(tagName).append
    }
#endif

    class XmlInput : public std::string {
    public:
        bool parse();
        tinyxml2::XMLError parseError() const;
        std::string parseErrorText() const;
        tinyxml2::XMLElement * root();

        XmlInput();
        ~XmlInput();
    protected:
        tinyxml2::XMLDocument * xmlDocument_;
        uint8_t error_;
    };


    class XmlRequest {
    public:
        //static XmlGen::XmlOutput begin(const char *requestName, bool closeTag = false);
        static void begin(XmlGen::XmlOutput *out,const char *requestName, bool closeTag = false);

        // execute and get raw unparsed XML response
        bool executeWithTextResponse();
        bool executeWithTextResponse(std::string &str);

        XmlRequest(TickDbImpl &db, const char * requestName, bool noContent = false);
        std::string end();

        // Public flag, do not throw exception if the server returned error in response, false by default
        bool noException;
        bool allowUnopenedDb;

    protected:
        bool executeAndParseResponse();
        virtual std::string getRequest();

        // Call to log response on error
        void responseParseError(const char *err);

        // Adding standard elements to the request
        //template <typename T> XmlGen::XmlOutput& operator<<(const T &value);
        template <typename T> XmlGen::XmlOutput& addItemArray(const char * tagName, const T[], size_t count);
        template <typename T> XmlGen::XmlOutput& addItemArray(const char * tagName, const T &value);
        XmlGen::XmlOutput& add(const char * name, const char * content);
        template <typename T> XmlGen::XmlOutput& add(const char * name, const T *content) = delete; // No pointers
        template <typename T> XmlGen::XmlOutput& add(const char * name, const T &content);
       

        // Private data fields
        XmlInput            response_;
        TickDbImpl          &db_;
        XmlGen::XmlOutput   request_;
        const char          *name_;

        // No content tags in request, just header
        bool noContent_;

        // Do not send the request, response string is already initialized
        bool noRequestNeeded_;

        ~XmlRequest();
    };


    // Inline and template method implementations

    namespace XmlGen {

        INLINE void makeRequestHeader(XmlOutput *out, const char *req, bool closeTag)
        {
            assert(NULL != out);
            (out->put("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>").put(CRLF).put('<').put(req).put(" version=\"")
                << TDB::PROTOCOL_VERSION).put('\"').put(closeTag ? "/>" CRLF : ">" CRLF);
        }

        // This version is disabled. Compiler/STL compatibility problem.
        /*INLINE XmlOutput makeRequestHeader(const char *req, bool closeTag)
        {
            XmlOutput out;
            makeRequestHeader(&out, req, closeTag);
            return out;
        }*/

        /*template <class T> INLINE std::string tag(const char * tagName, const T &content)
        {
            return std::string("<").append(tagName).append(">").append(toString(content)).append("</").append(tagName).append(">");
        }*/

        /*template <class T> INLINE std::string tag(const char * tagName, const DxApi::Nullable<T> &content)
        {
            return content.is_set() ? tag<T>(tagName, content.get()) : std::string("");
        }*/


        INLINE std::stringstream& XmlOutput::ostream()
        {
            return static_cast<std::stringstream&>(*this);
        }

        INLINE XmlOutput& XmlOutput::put(char data) { ::std::stringstream::put(data); return *this; }
        INLINE XmlOutput& XmlOutput::put(const char * data, size_t size) { this->write(data, size); return *this; }
        INLINE XmlOutput& XmlOutput::put(const char * str) { this->write(str, strlen(str)); return *this; }


        INLINE XmlOutput& XmlGen::XmlOutput::addChar(char c)
        {
            switch (c) {
            case '&':  put("&amp;", 5);       break;
            case '\"': put("&quot;", 6);      break;
            case '\'': put("&apos;", 6);      break;
            case '<':  put("&lt;", 4);        break;
            case '>':  put("&gt;", 4);        break;
            default:   put(c);                break;
            }

            return *this;
        }


        INLINE XmlOutput& XmlOutput::addChars(const char *src, size_t n)
        {
            src += n;
            for_neg(i, -(intptr_t)n) {
                addChar(src[i]);
            }

            return *this;
        }


        template<> INLINE XmlOutput& XmlOutput::add(const std::string &value)
        {
            return this->addChars(value.c_str(), value.length());
        }


        INLINE XmlOutput& XmlOutput::add(const char *value)
        {
            return this->addChars(value, strlen(value));
        }


        template <typename T> INLINE XmlOutput& XmlOutput::add(const T &value)
        {
            return *this << value;
        }


        template <typename T> INLINE XmlOutput& XmlOutput::add(const char * tagName, const T &content)
        {
            return (*this).openTag(tagName).add(content).closeTag(tagName);
        }


        INLINE XmlOutput& XmlOutput::add(const char * tagName, const char * content)
        {
            return (*this).openTag(tagName).add(content).closeTag(tagName);
        }


        template<typename T> INLINE XmlOutput& XmlOutput::addItems(const T items[], size_t count)
        {
            assert(NULL != items);
            forn(i, count) {
                openTag("item").add(items[i]).closeTag("item").add('\n');
            }

            return *this;
        }


        template <typename T> INLINE XmlOutput& XmlOutput::addItemArray(const char * name, const std::vector<T> &valuesVector)
        {
            size_t n = valuesVector.size();
            return addItemArray(name, data_ptr(valuesVector), n);
        }

        // TODO: improve
        template <typename P, typename Q> INLINE XmlOutput& XmlOutput::addItemMap(const char * name, const char * keyName, const char * valueName,
            const std::unordered_map<P, Q> &values)
        {
            openTag(name);
            for(auto &i : values) {
                openTag("item").add(keyName, i.first).add(valueName, i.second).closeTag("item").add('\n');
            }

            closeTag(name);
            return *this;
        }

        template <typename P, typename Q> INLINE XmlOutput& XmlOutput::addItemMap(const char * name, const char * keyName, const char * valueName,
            const DxApi::Nullable<std::unordered_map<P, Q>> &values)
        {
            return values.is_null() ? *this : this->addItemMap(name, keyName, valueName, values.get());
        }


        template <typename T> INLINE XmlOutput& XmlOutput::addItemArray(const char * name, const std::vector<T> * valuesVector)
        {
            if (NULL != valuesVector) {
                return addItemArray(name, *valuesVector);
            }
            else {
                return addItemArray(name, (const T *)NULL, 0);
            }
        }

        // Starting from 2015-12-08 server will distinguish between empty(==nothing) and null array (==all)
        // if content is null, we write nothing
        // if content has 0 length, we write empty tag
        template <typename T> INLINE XmlOutput& XmlOutput::addItemArray(const char * tagName, const T content[], size_t count)
        {
            return NULL != content ? (*this).openTag(tagName).addItems(content, count).closeTag(tagName).add('\n') : *this;
        }
    }

    //template<> XmlGen::XmlOutput& operator<<(XmlGen::XmlOutput &to, const DxApi::InstrumentIdentity &value);

    INLINE XmlGen::XmlOutput& operator<<(XmlGen::XmlOutput &to, const void * value) = delete;

    template<typename T> INLINE XmlGen::XmlOutput& operator<<(XmlGen::XmlOutput &to, const DxApi::Nullable<T> &value)
    {
        if (value.is_set()) {
            //to << value.get();
            //to.XmlOutput::operator<<(value.get());
            //to.add(value.get());
            to.add<T>(value.get());
        }

        return to;
    }
    

    INLINE XmlGen::XmlOutput& operator<<(XmlGen::XmlOutput &to, char value)
    {
        return to.addChar(value);
    }


    INLINE XmlGen::XmlOutput& operator<<(XmlGen::XmlOutput &to, const char * value)
    {
        return to.addChars(value, strlen(value));
    }


    INLINE XmlGen::XmlOutput& operator<<(XmlGen::XmlOutput &to, const std::string& value)
    {
        return to.addChars(value.data(), value.size());
    }


    INLINE XmlGen::XmlOutput& operator<<(XmlGen::XmlOutput &to, const std::stringstream& value)
    {
        return to << value.str();
    }


    INLINE XmlGen::XmlOutput& operator<<(XmlGen::XmlOutput &to, const bool value)
    {
        if (value) {
            to.put("true", 4);
        }
        else {
            to.put("false", 5);
        }

        return to;
    }


    /*template<typename T, class Q> INLINE XmlGen::XmlOutput& operator<<(XmlGen::XmlOutput &to, const DxApiImpl::EnumClass<T, Q> &value)
    {
        return (to << value.toString());
    }*/


    template<typename T> INLINE XmlGen::XmlOutput& operator<<(XmlGen::XmlOutput &to, const T &value)
    {
        to.ostream() << value;
        return to;
    }


    INLINE void XmlRequest::begin(XmlGen::XmlOutput *out, const char *requestName, bool closeTag)
    {
        XmlGen::makeRequestHeader(out, requestName, closeTag);
    }


    /*INLINE XmlGen::XmlOutput XmlRequest::begin(const char * requestName, bool closeTag)
    {
        return XmlGen::makeRequestHeader(requestName, closeTag);
    }*/


    template <typename T> INLINE XmlGen::XmlOutput& XmlRequest::add(const char * tagName, const T &value)
    {
        return request_.add(tagName, value);
    }

    
    template <typename T> INLINE XmlGen::XmlOutput& XmlRequest::addItemArray(const char * tagName, const T value[], size_t count)
    {
        return request_.addItemArray(tagName, value, count);
    }

    template <typename T> INLINE XmlGen::XmlOutput& XmlRequest::addItemArray(const char * tagName, const T &value)
    {
        return request_.addItemArray(tagName, value);
    }

    INLINE XmlGen::XmlOutput& XmlRequest::add(const char * tagName, const char * value)
    {
        return request_.add(tagName, value);
    }

}