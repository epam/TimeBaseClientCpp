#pragma once

#include "modify_subscription_request.h"

namespace DxApiImpl {
    class ModifyStreamsRequest : public ModifySubscriptionRequest {
    public:
        ModifyStreamsRequest(TickDbImpl &db, int64_t id, int64_t time, SubscrChangeAction changeMode, const DxApi::TickStream * const streams[], size_t numStreams)
            : ModifySubscriptionRequest(db, "changeStreams", id, time, changeMode)
        {
            addItemArray("streams", streams, numStreams);
        }
    };
}