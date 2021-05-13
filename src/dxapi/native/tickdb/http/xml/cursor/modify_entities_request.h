#pragma once

#include "modify_subscription_request.h"

namespace DxApiImpl {
    class ModifyEntitiesRequest : public ModifySubscriptionRequest {
    public:
        ModifyEntitiesRequest(TickDbImpl &db, int64_t id, int64_t time, SubscrChangeAction changeMode, const std::string entities[], size_t numEntities)
            : ModifySubscriptionRequest(db, "changeEntities", id, time, changeMode)
        {
            addItemArray("entities", entities, numEntities);
        }
    };
}