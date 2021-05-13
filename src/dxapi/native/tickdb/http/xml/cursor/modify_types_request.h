#pragma once

#include "modify_subscription_request.h"


namespace DxApiImpl {
    class ModifyTypesRequest : public ModifySubscriptionRequest {
    public:
        ModifyTypesRequest(TickDbImpl &db, int64_t id, int64_t time, SubscrChangeAction changeMode, const std::string types[], size_t numTypes)
            : ModifySubscriptionRequest(db, "changeTypes", id, time, changeMode)
        {
            addItemArray("types", types, numTypes);
        }
    };
}