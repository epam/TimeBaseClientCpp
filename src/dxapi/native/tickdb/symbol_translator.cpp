#include <dxapi/dxapi.h>
#include "symbol_translator.h"


using namespace std;
using namespace DxApi;
using namespace DxApiImpl;


typedef unordered_map<string, unsigned> Name2ID;


void SymbolTranslator::clear()
{
    instruments.clear();

    typeIdLocal2Remote.setDefaultValue(EMPTY_MSG_TYPE_ID);
    typeIdLocal2Remote.clear();

    typeIndex.clear();
    streamIndex.clear();

    /*remoteMessageTypes.clear();
    localMessageTypes.clear();*/

    forn(i, messageDescriptors.size()) {
        if (NULL != messageDescriptors[i]) {
            delete messageDescriptors[i];
        }
    }

    messageDescriptors.clear();

    forn(i, streamKeys_.size()) {
        if (NULL != streamKeys_[i]) {
            delete streamKeys_[i];
        }
    }

    streamKeys_.setDefaultValue(NULL);
    streamKeys_.clear();

    /*localMessageTypeNames.clear();
    messageTypeRemote2Local.clear();
    messageTypeLocal2Remote.clear();*/
}


//SymbolTranslator::SymbolTranslator(const SymbolTranslator &other)
//    :
//    symbols(other.symbols),
//    symbolTypes(other.symbolTypes),
//    remoteMessageTypes(other.remoteMessageTypes),
//    localMessageTypes(other.localMessageTypes),
//    messageTypeRemote2Local(other.messageTypeRemote2Local),
//    messageTypeLocal2Remote(other.messageTypeLocal2Remote),
//    remoteMessageTypeNames(remoteMessageTypeNames),
//    localMessageTypeNames(remoteMessageTypeNames)
//{
//    
//    
//}
//
//SymbolTranslator& SymbolTranslator::operator = (const SymbolTranslator &other)
//{
//    symbols = other.symbols;
//    symbolTypes = other.symbolTypes;
//    remoteMessageTypes = other.remoteMessageTypes;
//    localMessageTypes = other.localMessageTypes;
//    messageTypeRemote2Local = other.messageTypeRemote2Local;
//    messageTypeLocal2Remote = other.messageTypeLocal2Remote;
//    remoteMessageTypeNames = remoteMessageTypeNames;
//    localMessageTypeNames = remoteMessageTypeNames;
//
//    return *this;
//}



// The current implementation expects that the type is already known from parsed stream schema. Used in Loader
unsigned SymbolTranslator::registerLocalMessageType(unsigned newLocalMsgTypeId, const string &typeName)
{
    unsigned i_found;
    assert(NULL != &typeName);

    if (newLocalMsgTypeId >= typeIdLocal2Remote.maxSize()) {
        THROW_DBGLOG("registerMessageType: typeId is too big: %u", newLocalMsgTypeId);
    }

    i_found = findMsgDescByType(typeName);

    //localMessageTypes[newTypeName] = typeId;
    /*MessageInfo * found = NULL;
    intptr i_found = -1;
    forn (i, remoteMessageInfo.size()) {
        if (newTypeName == (found = remoteMessageInfo[i_found = i])->name) {
            break;
        }
    }*/

    if (!isRegisteredMsgDescId(i_found)) {
        THROW_DBGLOG("registerMessageType: type name '%s' not found among %u entries read from stream metadata",
            typeName.c_str(), (unsigned)messageDescriptors.size());
    }

    typeIdLocal2Remote.set(newLocalMsgTypeId, (unsigned)i_found);

    return newLocalMsgTypeId;
}


SymbolTranslator::MessageDescriptor::MessageDescriptor() : id(EMPTY_MSG_TYPE_ID), nextSameType(EMPTY_MSG_TYPE_ID)
{
}


const SymbolTranslator::MessageDescriptor * SymbolTranslator::registerMessageDescriptor(unsigned msgDescId, /*const std::string &streamKey,*/ const std::string &typeName, const std::string &guid, const std::string &schema)
{
    MessageDescriptor * msgDesc;
    // TODO: for these functions, test if the pair already exists and not equal to this one
    if (msgDescId >= 0x100) {
        THROW_DBGLOG("registerRemoteMessageType: Message Descriptor Id is too big: %u", msgDescId);
    }

    //msgDesc = messageDescriptors[msgDescId];
    // Message info entry is never overwritten. Actually, this check and the previous one is redundant anyway, but gives more debug info
    //if (NULL != messageInfo) {
    //    THROW_DBGLOG("registerRemoteMessageType(): ERROR: message info already exists");
    //    //delete messageInfo;
    //}

    if (msgDescId != messageDescriptors.size()) {
        THROW_DBGLOG("registerRemoteMessageType(): ERROR: invalid registration order. Got %u, expected %u", (unsigned)msgDescId, (unsigned)messageDescriptors.size());
    }


    msgDesc = new MessageDescriptor();
    messageDescriptors.push_back(msgDesc);
    assert(messageDescriptors.size() == msgDescId + 1);

    msgDesc->id = msgDescId;
    msgDesc->guid = guid;
    msgDesc->typeName = typeName;
    msgDesc->schema = schema;

    unsigned previous = msgDesc->nextSameType = findMsgDescByType(typeName);
    typeIndex[typeName] = msgDescId;
    if (EMPTY_MSG_TYPE_ID == previous) {
        typeList.push_back(msgDesc);
    }

    /*previous = msgDesc->nextSameStream = findMsgDescByStreamKey(streamKey);
    streamIndex[streamKey] = msgDescId;
    if (DxApi::emptyMsgDescId == previous) {
        streamList.push_back(msgDesc);
    }*/

    assert(typeIndex.size() == typeList.size());
    /*assert(streamIndex.size() == streamList.size());*/


    /*remoteMessageTypes[newTypeName] = typeId;
    Name2ID::const_iterator i = localMessageTypes.find(newTypeName);
    unsigned localTypeId = localMessageTypes.cend() == i ? emptyLocalTypeId : i->second;

    setLUTsFromRemote(typeId, localTypeId, newTypeName);

    if (isValidLocalTypeId(localTypeId))
        setLUTsFromLocal(localTypeId, typeId, newTypeName);*/

    return msgDesc;
}


bool SymbolTranslator::registerInstrument(unsigned entityId, const std::string &instr)
{
    // TODO: for these functions, test if the pair already exists and not equal to this one
    if (entityId > 0xFFFF) {
        THROW_DBGLOG("registerInstrument: entityID is too big: %u", entityId);
    }

    auto prevEntityId = instruments[instr];
    if (prevEntityId != entityId) {
        if (prevEntityId != instruments.invalidId()) {
            // TODO: Expand
            THROW_DBGLOG("InstrumentId cache out of sync");
        }

        // Register new entity
        instruments.add(entityId, instr);
        return true;
    }

    return false;
}