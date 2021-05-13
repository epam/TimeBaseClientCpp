#include "tickdb/common.h"
#include <dxapi/dxapi.h>

#include <algorithm>

#include "io/input_preloader.h"

#include "xml/selection_options_impl.h"
#include "xml/xml_request.h"
#include "xml/tickdb_class_descriptor.h"
#include "xml/cursor/reset_request.h"
#include "xml/cursor/close_request.h"
#include "xml/cursor/modify_entities_request.h"
#include "xml/cursor/modify_types_request.h"
#include "xml/cursor/modify_streams_request.h"

#include "tickdb/tickstream_impl.h"

#include "tickdb_http.h"
#include "tickcursor_http.h"
#include "util/static_array.h"


using namespace std;
using namespace DxApi;
using namespace DxApiImpl;
using namespace DxApiImpl::TDB;
using namespace dx_thread;

#define THIS_IMPL (impl(this))


#if VERBOSE_CURSOR_FILTERS >= 1
#define DBGLOG_VERBOSE DBGLOG
#else
#define DBGLOG_VERBOSE (void)
#endif

#define LOGHDR "%s"
#define ID (this->textId().c_str())


#if VERBOSE_CURSOR_FILTERS >= 2
#define DBGLOG_MSG DBGLOG
#define DBG_DUMP dbg_dump
#else
#define DBGLOG_MSG (void)
#define DBG_DUMP (void)
#endif


const char *infoSubscrChangeAction[] = {
    "SET",
    "ADD",
    "REMOVE"
};

IMPLEMENT_ENUM(uint8_t, SubscrChangeAction, false)
{}


void TickCursorImpl::addSubscriptionCommand(SubscriptionState &s, int64_t cmdId, const char * requestName)
{
    // This function is always called under thisLock
    if (-1 == s.lastExpectedCommandId_) {
        s.firstExpectedCommandId_ = s.lastExpectedCommandId_ = cmdId;
    }
    else {
        int64 expected = s.lastExpectedCommandId_ + 1;
        assert(-1 != s.firstExpectedCommandId_);
        assert(s.firstExpectedCommandId_ < expected);
        if (cmdId != expected) {
            THROW_DBGLOG(LOGHDR ".addCommandId(%s): Command Id sequence is broken %lld received, %lld expected", ID, requestName, (longlong)cmdId, (longlong)expected);
        }

        s.lastExpectedCommandId_ = expected;
    }

    if (NULL != requestName) {
        DBGLOG(LOGHDR ": %s request submitted, id: %lld", ID, requestName, (longlong)cmdId);
    }

    // TODO: Atomic?
    if (state <= CursorState::END) {
        setState(CursorState::STARTED);
    }

    s.readingRestarted_ = true;
    DBGLOG(LOGHDR ".addSubscriptionCommand(): Subscription updated, state set to STARTED", ID);
}


// Subscription methods go here

void TickCursorImpl::SubscriptionState::clearMessageFilters(bool isSkippingAll)
{
    isMessageDescriptorSkipped_.setDefaultValue(isSkippingAll).clear();
    isSymbolSkipped_.setDefaultValue(isSkippingAll).clear();
    isStreamSkipped_.setDefaultValue(isSkippingAll).clear();
    isUnregisteredSymbolSkipped_.clear();
    isUnregisteredMessageTypeSkipped_.clear();
    isUnregisteredStreamSkipped_.clear();
}


//void TickCursorImpl::SubscriptionState::parseStreamGuids(const DxApi::TickStream * stream)
//{
//    const string &key = stream->key();
//    if (stream->options().metadata.is_null()) {
//        return;
//    }
//
//    auto guids = Schema::TickDbClassDescriptor::guidsFromXML(stream->options().metadata.get());
//    for (auto &i : guids) {
//        guid2streamKey_[i.second] = key;
//    }
//}
//
//
//void TickCursorImpl::SubscriptionState::parseStreamGuids()
//{
//    for (auto i : tickdbStreams_) {
//        parseStreamGuids(i);
//    }
//}


void TickCursorImpl::reset(TimestampMs dt)
{
    assert(cursorId_ >= 0);

    srw_write section(thisLock_);
    SubscriptionState &s = getWriteShadowState();

    s.isResetInProgress_ = true;
    
    // Since the reset mode is activated, we don't need filtering anymore, the server will filter messages for us after reset
    s.clearMessageFilters(false);

    DBGLOG_VERBOSE(LOGHDR ".reset(%s)", ID, TS_CURSOR2STR(dt));
    subscriptionTimeMs_ = USE_CURSOR_TIME;

    ResetRequest req(db_, cursorId_, dt);
    addSubscriptionCommand(s, req.execute(), "reset");
    s.lastResetCommandId_ = s.lastExpectedCommandId_;
}


//void convert_types(std::vector<InstrumentType> & out, const std::string types[], size_t numTypes)
//{
//    out.clear();
//    if (nullptr == types || 0 == numTypes) return;
//    out.reserve(numTypes);
//
//    forn(i, numTypes) {
//        InstrumentType t = InstrumentType::fromString(types[i]);
//        if (InstrumentType::UNKNOWN == t) {
//            THROW_DBGLOG("Unknown InstrumentType name: %s", types[i].c_str());
//        }
//        out.push_back(t);
//    }
//}

static char action_str[4] = { '=', '+', '-', '\0' };

void TickCursorImpl::modifyEntities(const std::string entities[], size_t numEntities, SubscrChangeAction action)
{
    assert(cursorId_ >= 0);


    /*if (SubscrChangeAction::SET == action) {
        modifyEntities(NULL, 0, SubscrChangeAction::REMOVE);
        action = SubscrChangeAction::ADD;
        if (0 == numEntities) {
            return;
        }
    }*/

    DBGLOG_VERBOSE(LOGHDR ".modifyEntities(%s: %c %s)", ID, TS_CURSOR2STR(subscriptionTimeMs_), action_str[(uintptr)action], subscriptionToString(entities, numEntities).c_str());

    srw_write section(thisLock_);
    SubscriptionState &s = getWriteShadowState();
    auto &filter = s.isSymbolSkipped_;
    bool isRemoving = SubscrChangeAction::REMOVE == action;

    if (NULL == entities || SubscrChangeAction::SET == action) {
        // 'Remove all' for 'remove all' & 'set', otherwise 'add all'
        filter.setDefaultValue(SubscrChangeAction::ADD != action).clear();
        s.isUnregisteredSymbolSkipped_.clear();
    }
    
    if (NULL != entities) {
        forn(i, numEntities) {
            const std::string &e = entities[i];
            auto entityId = s.tables_.instruments[e];
            if (s.tables_.instruments.invalidId() != entityId) {
                filter.set(entityId, isRemoving);
            }
            else if (filter.defaultValue() != (uint8_t)isRemoving) {
                s.isUnregisteredSymbolSkipped_[e] = isRemoving;
            }
            else {
                // Do nothing, this case is covered by the default filter setting
            }
        }
    }

   
#if !(DBG_SKIP_SUBSCRIPTION_CHANGE_REQUESTS > 0)
    ModifyEntitiesRequest req(db_, cursorId_, subscriptionTimeMs_, action, entities, numEntities);
    addSubscriptionCommand(s, req.execute(), "changeEntities");
#endif

    //DBGLOG(LOGHDR "changeEntities request completed", ID);
}


void TickCursorImpl::modifyEntities(const std::vector<std::string> &entities, SubscrChangeAction action)
{
    auto sz = entities.size();
    return modifyEntities(data_ptr(entities), sz, action);
    //return modifyEntities(0 != sz ? &entities[0] : NULL, sz, action);
}


void TickCursorImpl::modifyFilterSet(
    ArrayLUT<uint8_t, 0, 0x100> &filter,
    const_static_array_view<SymbolTranslator::MessageDescriptor *, 0x100> descriptors,
    size_t index,
    uint8_t value)
{
    assert(index < descriptors.size());
    assert(index < filter.maxSize());

    for (; index < filter.maxSize(); index = descriptors[index]->nextSameType) {
        assert(index < descriptors.size());
        filter.set(index, value);
    }
}


void TickCursorImpl::modifyTypes(const string types[], size_t numTypes, SubscrChangeAction action)
{
    assert(cursorId_ >= 0);

    /*if (SubscrChangeAction::SET == action) {
        this->modifyTypes((const std::string *)NULL, 0, SubscrChangeAction::REMOVE);
        action = SubscrChangeAction::ADD;
        if (0 == numTypes) {
            return;
        }
    }*/

#if VERBOSE_CURSOR_FILTERS >= 1
    {
        DBGLOG_VERBOSE(LOGHDR ".modifyTypes(%s: %c %s)", ID, TS_CURSOR2STR(subscriptionTimeMs_), action_str[(uintptr)action], subscriptionToString(types, numTypes).c_str());
    }
#endif

    bool isRemoving = SubscrChangeAction::REMOVE == action;

    srw_write section(thisLock_);
    SubscriptionState &s = getWriteShadowState();
    auto &filter = s.isMessageDescriptorSkipped_;

    // Remove entities if necessary
    if (NULL == types || SubscrChangeAction::SET == action) {
        // 'Remove all' for 'remove all' & 'set', otherwise 'add all'
        filter.setDefaultValue(SubscrChangeAction::ADD != action).clear();
        s.isUnregisteredMessageTypeSkipped_.clear();
    }

    if (NULL != types) {
        forn(i, numTypes) {
            const string &t = types[i];
            auto msgTypeId = s.tables_.findMsgDescByType(t);
            if (EMPTY_MSG_TYPE_ID != msgTypeId) {
                modifyFilterSet(filter, s.tables_.messageDescriptors, msgTypeId, (uint8_t)isRemoving);
            }
            else {
                if (filter.defaultValue() != (uint8_t)isRemoving) {
                    // Save this typename for later (we have yet to encounter a message with this type)
                    s.isUnregisteredMessageTypeSkipped_[t] = isRemoving;
                }

                // Otherwise do nothing, this case is covered by the default filter setting
            }
        }
    }

#if !(DBG_SKIP_SUBSCRIPTION_CHANGE_REQUESTS > 0)
    ModifyTypesRequest req(db_, cursorId_, subscriptionTimeMs_, action, types, numTypes);
    addSubscriptionCommand(s, req.execute(), "changeTypes");
#endif

    //DBGLOG(LOGHDR "changeTypes request completed", ID);
}


void TickCursorImpl::modifyTypes(const vector<string> &types, SubscrChangeAction action)
{
    auto sz = types.size();
    return modifyTypes(data_ptr(types), sz, action);
}


void TickCursorImpl::modifyTypes(const char * types[], size_t numTypes, SubscrChangeAction action)
{
    if (NULL == types) {
        return this->modifyTypes((const string *)NULL, numTypes, action);
    }
    vector<string> t(numTypes);
    
    forn(i, numTypes) {
        t[i] = types[i];
    }

    return this->modifyTypes(t.data(), numTypes, action);
}


void TickCursorImpl::modifyStreams(const TickStream * const streams[], size_t numStreams, SubscrChangeAction action)
{
    assert(cursorId_ >= 0);

    // modifyStreams retains remove+add logic for 'set' operation
    // Will need additional work when this is changed
    if (SubscrChangeAction::SET == action) {
        modifyStreams(NULL, 0, SubscrChangeAction::REMOVE);
        action = SubscrChangeAction::ADD;
        if (0 == numStreams) {
            return;
        }
    }

#if VERBOSE_CURSOR_FILTERS >= 1
    {
        DBGLOG_VERBOSE(LOGHDR ".modifyStreams(%s: %c %s)", ID, TS_CURSOR2STR(subscriptionTimeMs_), action_str[(uintptr)action], subscriptionToString(streams, numStreams).c_str());
    }
#endif

    bool isRemoving = SubscrChangeAction::REMOVE == action;
    srw_write section(thisLock_);
    SubscriptionState &s = getWriteShadowState();

    DBGLOG(LOGHDR ".modifyStreams(): preparing to change subscription state", ID);

    std::vector <const DxApi::TickStream *> newStreamList;
    newStreamList.push_back(NULL);

    if (NULL == streams) {
        s.isStreamSkipped_.setDefaultValue(SubscrChangeAction::ADD != action).clear();
        s.isUnregisteredStreamSkipped_.clear();
    }

    if (NULL != streams) {
        forn(i, numStreams) {
            const TickStream *stream = streams[i];
            const string &key = impl(stream)->key();

            auto streamId = s.tables_.findStreamId(key);
            if (DxApi::emptyStreamId != streamId) {
                s.isStreamSkipped_.set((unsigned)streamId, isRemoving);
            }
            else if (s.isStreamSkipped_.defaultValue() != (uint8_t)isRemoving) {
                // Save this stream key for later (we have yet to encounter a message from this stream)
                s.isUnregisteredStreamSkipped_[key] = isRemoving;
            }
            else {
                // Do nothing, this case is covered by the default filter setting
            }

            // Update main stream list
            bool shouldAdd = !isRemoving;
            for (auto &j : s.tickdbStreams_) {
                if (NULL != j) {
                    //if (impl(*i).key() == key) {
                    if (j == stream) {
                        if (isRemoving) {
                            j = NULL;
                        }

                        shouldAdd = false;
                        break;
                    }
                }
            }

            if (shouldAdd) {
                s.tickdbStreams_.push_back(stream);
            }
        }
    }

    // Re-add all non-null streams
    for (auto i : s.tickdbStreams_) {
        if (NULL != i) {
            newStreamList.push_back(i);
        }
    }

    // Set the new stream list
    s.tickdbStreams_ = newStreamList;

#if !(DBG_SKIP_SUBSCRIPTION_CHANGE_REQUESTS > 0)
    ModifyStreamsRequest req(db_, cursorId_, subscriptionTimeMs_, action, streams, numStreams);
    addSubscriptionCommand(s, req.execute(), "changeStreams");
#endif

    // DBGLOG(LOGHDR ".modifyStreams(): changeStreams sent and applied", ID);
}


// Proxy methods

// Reset with entities is implemented as setEntities() & reset()
void TickCursor::reset(TimestampMs dt, const vector<std::string> &entities)
{
    auto sz = entities.size();
    if (0 != sz) {
        THIS_IMPL->modifyEntities(data_ptr(entities), sz, SubscrChangeAction::SET);
    }

    THIS_IMPL->reset(dt);
}


void TickCursor::reset(TimestampMs dt)
{
    THIS_IMPL->reset(dt);
}


void TickCursor::addEntities(const vector<std::string> &entities)
{
    THIS_IMPL->modifyEntities(entities, SubscrChangeAction::ADD);
}


void TickCursor::addEntities(std::string entities[], size_t n)
{
    THIS_IMPL->modifyEntities(entities, n, SubscrChangeAction::ADD);
}


void TickCursor::addEntity(const std::string &entity)
{
    THIS_IMPL->modifyEntities(&entity, 1, SubscrChangeAction::ADD);
}


void TickCursor::removeEntities(const vector<std::string> &entities)
{
    THIS_IMPL->modifyEntities(entities, SubscrChangeAction::REMOVE);
}


void TickCursor::removeEntities(std::string entities[], size_t n)
{
    THIS_IMPL->modifyEntities(entities, n, SubscrChangeAction::REMOVE);
}


void TickCursor::removeEntity(const std::string &entity)
{
    THIS_IMPL->modifyEntities(&entity, 1, SubscrChangeAction::REMOVE);
}


void TickCursor::clearAllEntities()
{
    THIS_IMPL->modifyEntities(NULL, 0, SubscrChangeAction::REMOVE);
}


void TickCursor::subscribeToAllEntities()
{
    THIS_IMPL->modifyEntities(NULL, 0, SubscrChangeAction::ADD);
}


void TickCursor::addTypes(const vector<string> &types)
{
    THIS_IMPL->modifyTypes(types, SubscrChangeAction::ADD);
}


void TickCursor::addTypes(const char * types[], size_t n)
{
    THIS_IMPL->modifyTypes(types, n, SubscrChangeAction::ADD);
}


void TickCursor::removeTypes(const vector<string> &types)
{
    THIS_IMPL->modifyTypes(types, SubscrChangeAction::REMOVE);
}


void TickCursor::removeTypes(const char * types[], size_t n)
{
    THIS_IMPL->modifyTypes(types, n, SubscrChangeAction::REMOVE);
}



void TickCursor::setTypes(const vector<string> &types)
{
    THIS_IMPL->modifyTypes(types, SubscrChangeAction::SET);
}


void TickCursor::setTypes(const char * types[], size_t n)
{
    THIS_IMPL->modifyTypes(types, n, SubscrChangeAction::SET);
}


void TickCursor::subscribeToAllTypes()
{
    addTypes((const char **)NULL, 0);
}


void TickCursor::add(const vector<std::string> &entities, const vector<string> &types)
{
    this->addEntities(entities);
    this->addTypes(types);
}


void TickCursor::remove(const vector<std::string> &entities, const vector<string> &types)
{
    this->removeEntities(entities);
    this->removeTypes(types);
}


void TickCursor::addStreams(const vector<TickStream *> &streams)
{
    THIS_IMPL->modifyStreams(data_ptr(streams), streams.size(), SubscrChangeAction::ADD);
}


void TickCursor::removeStreams(const vector<TickStream *> &streams)
{
    THIS_IMPL->modifyStreams(data_ptr(streams), streams.size(), SubscrChangeAction::REMOVE);
}


void TickCursor::removeAllStreams()
{
    THIS_IMPL->modifyStreams(NULL, 0, SubscrChangeAction::REMOVE);
}


void TickCursorImpl::setTimeForNewSubscriptions(TimestampMs dt)
{
    DBGLOG_VERBOSE(LOGHDR ".setTimeForNewSubscriptions(%s)", ID, TS_CURSOR2STR(dt));
    subscriptionTimeMs_ = dt;
}


void TickCursor::setTimeForNewSubscriptions(TimestampMs dt)
{
    THIS_IMPL->setTimeForNewSubscriptions(dt);
}
