#pragma once

#include "dxcommon.h"
#include "dxconstants.h"


namespace DxApi {

    struct ExecutionStatusEnum {
        enum Enum {
            NONE,
            RUNNING,
            COMPLETED,
            ABORTED,
            FAILED
        };
    };

    ENUM_CLASS(uint8_t, ExecutionStatus);

    class BackgroundProcessInfo {
    public:
        std::string name;
        ExecutionStatus status;

        // keys of affected streams
        // DxTickStream instead of string? 
        std::vector<std::string> affectedStreams;
        double progress;
        TimestampMs startTime;
        TimestampMs endTime;

        bool isFinished() const
        {
            return status == ExecutionStatus::COMPLETED     ||
                   status == ExecutionStatus::ABORTED       ||
                   status == ExecutionStatus::FAILED;
        }

    public:
        BackgroundProcessInfo() :
            status(ExecutionStatus::NONE), progress(0.0), startTime(TIMESTAMP_NULL), endTime(TIMESTAMP_NULL)
        {}
    };
}
