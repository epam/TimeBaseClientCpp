#include "xml_request.h"
#include "stream_options_impl.h"


using namespace std;
using namespace DxApi;
using namespace DxApiImpl;

const char * infoStreamScope[] = {
    "DURABLE",
    "EXTERNAL_FILE",
    "TRANSIENT",
    "RUNTIME"
};

IMPLEMENT_ENUM(uint8_t, StreamScope, false)
{}

#define ADD(x) xml.add(#x, v.x).add('\n')

//#define ADD(x) do { ss << tag(#x, x).append("\n"); } while (0)


using namespace XmlGen;
namespace DxApiImpl {
    template<> XmlOutput& operator<<(XmlOutput& xml, const BufferOptionsImpl &v)
    {
        xml.add('\n');
        ADD(initialBufferSize);
        ADD(maxBufferSize);
        ADD(maxBufferTimeDepth);
        ADD(lossless);

        return xml;
    }


    template<> XmlOutput& operator<<(XmlOutput& xml, const StreamOptionsImpl &v)
    {
        xml.add('\n');

        // TODO: Remove extra checks, nullable should be supported already
        if (v.name.is_set()) {
            ADD(name);
        }

        if (v.description.is_set()) {
            ADD(description);
        }

        if (v.location.is_set()) {
            ADD(location);
        }

        ADD(distributionFactor);
        
        //ADD(bufferOptions);
        xml.add("bufferOptions", impl(v.bufferOptions)).add('\n');

        ADD(unique);
        ADD(duplicatesAllowed);

        // TODO: Not working on server
        ADD(scope);

        // TODO: switch periodicity to enum
        ADD(periodicity);

        if (v.owner.is_set()) {
            ADD(owner);
        }

        // TODO: should I write this or not?
        //ADD(highAvailability);

        ADD(polymorphic);

        ADD(metadata);
        // TODO: code cleanup
        //xml.openTag("metadata").addChars(v.metadata.get().c_str(), v.metadata.get().length()).closeTag("metadata");

        return xml;
    }
}


using namespace XmlParse;

#define GET(FIELDNAME) tmp = getText(root, #FIELDNAME); if(NULL != tmp) if (!parse(this->FIELDNAME, tmp)) break;


bool BufferOptionsImpl::fromXML(tinyxml2::XMLElement * root)
{
    const char * tmp;
    
    while (1) {
        GET(initialBufferSize);
        GET(maxBufferSize);
        GET(maxBufferTimeDepth);
        GET(lossless);
        return true;
    }
    
    return false;
}


// TODO: deprecated, remove
void copy_and_fix_metadata(string &dest, const string &src)
{
    std::string tmp1, tmp2;
    size_t l1 = src.size();
    //DBGLOG("%llu", src.size());
    // Horrible hack. Temporary solution to the namespace problem
    replace_all(&tmp1, src, "<ns2:", "<");
    replace_all(&tmp2, tmp1, "</ns2:", "</");
    replace_all(&tmp1, tmp2, "\"ns2:", "\"");
    replace_all(&tmp2, tmp1, "xmlns:ns2=", "xmlns=");
    replace_all(&tmp1, tmp2, " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"", "");
    replace_all(&tmp2, tmp1, "<classDescriptor ", "<classDescriptor xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" ");
    
    dest = std::move(tmp2);
    //DBGLOG("%llu", dest.size());

    if (dest.size() != l1) {
        DBGLOG("WARNING! options.metadata is still broken");
    }
    /*else {
#ifdef _DEBUG
        DBGLOG("options.metadata is fixed!!!");
#endif
    }*/
}


bool StreamOptions::operator==(const StreamOptions &other) const
{
    return name == other.name
        && description == other.description
        && owner == other.owner
        && scope == other.scope
        && distributionFactor == other.distributionFactor
        && distributionRuleName == other.distributionRuleName
        && location == other.location
        && duplicatesAllowed == other.duplicatesAllowed
        && highAvailability == other.highAvailability
        && unique == other.unique
        && polymorphic == other.polymorphic
        && metadata == other.metadata
        && bufferOptions == other.bufferOptions;
    // TODO: compare periodicity??
}

     
bool StreamOptionsImpl::fromXML(tinyxml2::XMLElement * root)
{
    const char * tmp;

    while (1) {
        GET(name);
        GET(description);
        GET(owner);
        GET(location);

        
        GET(metadata);
        /*if (metadata.is_set()) {
            string tmp;
            copy_and_fix_metadata(tmp, metadata.get());
            metadata = std::move(tmp);
        }*/

        GET(distributionFactor);

        GET(unique);
        GET(duplicatesAllowed);

        // TODO: Not working on server
        GET(scope);

        // TODO: periodicity to enum
        GET(periodicity);

        GET(highAvailability);

        GET(polymorphic);

        BufferOptionsImpl boNew;
        tinyxml2::XMLElement * e = root->FirstChildElement("bufferOptions");
        if (NULL != e) {
            if (!boNew.fromXML(e)) {
                return false;
            }
        }
        bufferOptions = boNew;

        return true;
    }
    return false;
}

