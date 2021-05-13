#pragma once

#include "tickdb/common.h"
#include <dxapi/dxconstants.h>

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "util/dynamic_lut.h"
#include "util/bidirectional_lut.h"
#include "util/hybrid_lut.h"
#include "util/static_array.h"


namespace DxApiImpl {
    // Growable lookup table with growth limit
    
    typedef BiLUT<std::string, uint32_t, 0> SymbolLUT;

 

    class SymbolTranslator {
        typedef std::unordered_map<std::string, uint32_t> Name2IdMap;

    public:
        struct MessageDescriptor {
                // ID of this message (index in the containing array)
            uint32_t id;                    

                // ID of previously defined message with same type name, value > 0xFF means NULL
                // messages with equal type name form a linked list
            uint32_t nextSameType;

            std::string typeName;
            std::string guid;
            std::string schema;
            MessageDescriptor();
        };

    public:
        const std::string& getInstrument(uintptr_t entityId) const;
        bool isRegisteredInstrumentId(uintptr_t entityId) const;
        
        // Convert string to Id, if present
        unsigned findInstrument(const std::string&s) const;

        // Return true if new conversion was added, false if already registered with same name and type
        //bool registerRemoteEntity(unsigned entityId, byte type, const std::string &name);
        bool registerInstrument(unsigned entityId, const std::string &instr);


        // Check, if the Id is valid and references previously registered symbol/message type
        bool isValidMsgDescId(uintptr_t msgDescId) const;
        bool isRegisteredMsgDescId(uintptr_t msgDescId) const;
        bool isRegisteredStreamId(uintptr_t streamId) const;

        // retrieve message descriptor for an id. returns null if not found
        const MessageDescriptor * getMessageDescriptor(uintptr_t msgDescId) const;
        MessageDescriptor * getMessageDescriptor(uintptr_t msgDescId);

        // retrieves stream key for a mesage descriptor id. Will only return NULL if the server error occured or the message is not server-originated 
        const std::string * getMessageTypeName(uintptr_t msgDescId) const;

        // retrieves typename for a mesage descriptor id. Will only return NULL if the server error occured or the message is not server-originated 
        //const std::string * getMessageStreamKey(uintptr_t msgDescId) const;


        const std::string * getMessageSchema(uintptr_t msgDescId) const;

        const MessageDescriptor * registerMessageDescriptor(unsigned newMsgDescId, /*const std::string &streamKey,*/ const std::string &typeName, const std::string &guid, const std::string &schema);


        unsigned findMsgDescByType(const std::string &s) const;
        //unsigned findMsgDescByStreamKey(const std::string &s) const;
        int findStreamId(const std::string &s) const;


        void clear();


        // Check id for invalid value only
        

        //unsigned findMessageLocalId(const std::string &s) const;
        //unsigned findMessageRemoteId(const std::string &s) const;

        

        // Retrieves message type for a given id

        //unsigned messageTypeToLocal(unsigned localTypeId) const { return messageType[msgTypeId]; }
       

        // Needed for loader

        // Registers user's message type. By default, ids 0 .. size-1 are assigned. any remote id > size-1 is considered invalid value. Typically uint32max is used when returning such value
        unsigned registerLocalMessageType(unsigned newLocalMsgTypeId, const std::string &typeName);

        bool isRegisteredLocalTypeId(uintptr_t typeId) const;


        SymbolTranslator() {}
        /*SymbolTranslator(const DxApiImpl::SymbolTranslator &other);
        SymbolTranslator& operator=(const DxApiImpl::SymbolTranslator &other);*/


    public:
        SymbolLUT instruments;
        
        static_array<MessageDescriptor *, 0x100, nullptr> messageDescriptors;      // registered remote message descriptor ID -> descriptor record. strong reference (owns MessageDescriptor structures)
        static_array<MessageDescriptor *, 0x100, nullptr> typeList;                // lists message descriptors with unique typeNames. weak reference
        //static_array<MessageDescriptor *, 0x100, nullptr> streamList;              // lists message descriptors with unique stream keys. weak reference

        Name2IdMap typeIndex;        // qs string TypeName     —> desc. id of the 1st MessageDescriptor with given type (negative values impossible)


        Name2IdMap streamIndex;      // qs string StreamKey    —> stream key


        ArrayLUT<std::string *, 0, 0x100> streamKeys_; // stream id -> stream key

        // Name2IdMap localMessageTypes;       // qs string typeID —> valid user typeID (negative values impossible)
        //std::vector<std::string *> localMessageTypeNames;                         // registered local typeID  -> name string
        //std::vector<std::unique_ptr<std::string>> remoteMessageTypeNames;         // registered remote typeID -> name string
        //std::vector<std::unique_ptr<std::string>> localMessageTypeNames;          // registered local typeID  -> name string

        VectorLUT<uint32_t, 0, 0x4000> typeIdLocal2Remote;                        // registered local typeID -> remote type Id, or default value, for TickLoader
        

    protected:
        void setLUTsFromRemote(unsigned from, unsigned to, const std::string &toName);
        void setLUTsFromLocal(unsigned from, unsigned to, const std::string &toName);

        //DISALLOW_COPY_AND_ASSIGN(SymbolTranslator);
    }; // class SymbolTranslator


    INLINE const std::string& SymbolTranslator::getInstrument(uintptr_t entityId) const { return *instruments[entityId]; }


    // retrieves name for remote type. Will only return NULL if the server error occured or the message is not server-originated 
    /*INLINE const std::string * SymbolTranslator::getMessageStreamKey(uintptr_t msgDescId) const
    {
        return msgDescId < messageDescriptors.size() ? &messageDescriptors[msgDescId]->streamKey : NULL;
    }*/


    // retrieves name for remote type. Will only return NULL if the server error occured or the message is not server-originated 
    INLINE const std::string * SymbolTranslator::getMessageTypeName(uintptr_t msgDescId) const
    {
        return msgDescId < messageDescriptors.size() ? &messageDescriptors[msgDescId]->typeName : NULL;
    }


    INLINE const SymbolTranslator::MessageDescriptor * SymbolTranslator::getMessageDescriptor(uintptr_t msgDescId) const
    {
        return msgDescId < messageDescriptors.size() ? messageDescriptors[msgDescId] : NULL;
    }


    INLINE const std::string * SymbolTranslator::getMessageSchema(uintptr_t msgDescId) const
    {
        return msgDescId < messageDescriptors.size() ? &messageDescriptors[msgDescId]->schema : NULL;
    }


    /*INLINE SymbolTranslator::MessageDescriptor * SymbolTranslator::getMessageDescriptor(uintptr_t msgDescId)
    {
        return msgDescId < messageDescriptors.size() ? messageDescriptors[msgDescId] : NULL;
    }*/


    INLINE bool SymbolTranslator::isValidMsgDescId(uintptr_t msgDescId) const { return DxApi::EMPTY_MSG_TYPE_ID != msgDescId; }

    INLINE bool SymbolTranslator::isRegisteredMsgDescId(uintptr_t msgDescId) const { return msgDescId < messageDescriptors.size()/* && NULL != remoteMessageInfo[msgDescId]*/; }

    INLINE bool SymbolTranslator::isRegisteredStreamId(uintptr_t streamId) const { return streamId < streamKeys_.size(); }

    // Only used by loader
    INLINE bool SymbolTranslator::isRegisteredLocalTypeId(uintptr_t localTypeId) const { return isValidMsgDescId(typeIdLocal2Remote.get(localTypeId)); }

    INLINE bool SymbolTranslator::isRegisteredInstrumentId(uintptr_t entityId) const { return instruments.contains(entityId); }


    INLINE unsigned SymbolTranslator::findInstrument(const std::string &s) const { return instruments[s]; /* invalidId if not found  */ }


    INLINE unsigned SymbolTranslator::findMsgDescByType(const std::string &s) const
    {
        Name2IdMap::const_iterator i = typeIndex.find(s);
        return typeIndex.cend() == i ? DxApi::EMPTY_MSG_TYPE_ID : i->second;
    }


    INLINE int SymbolTranslator::findStreamId(const std::string &s) const
    {
        Name2IdMap::const_iterator i = streamIndex.find(s);
        return streamIndex.cend() == i ? DxApi::emptyStreamId : (int32_t)i->second;
    }

    /*
    INLINE unsigned SymbolTranslator::findMessageLocalId(const std::string &s) const
    {
        Name2IdMap::const_iterator i = localMessageTypes.find(s);
        return localMessageTypes.cend() == i ? 0 : i->second;
    }


    INLINE unsigned SymbolTranslator::findMessageRemoteId(const std::string &s) const
    {
        Name2IdMap::const_iterator i = remoteMessageTypes.find(s);
        return remoteMessageTypes.cend() == i ? DxApi::emptyRemoteTypeId : i->second;
    }*/
}