#include "tickdb/common.h"
#include <dxapi/dxapi.h>

#include "tickstream_properties.h"

using namespace DxApiImpl;
using namespace DxApiImpl::TDB;

const char * infoTickStreamPropertyId[] = {
    "!!UNKNOWN!!",
    "KEY",
    "NAME",
    "DESCRIPTION",
    "PERIODICITY",
    "-SKIPPED-",
    "SCHEMA",
    "TIME_RANGE",
    "ENTITIES",
    "HIGH_AVAILABILITY",
    "UNIQUE",
    "BUFFER_OPTIONS",
    "VERSIONING",
    "DATA_VERSION",
    "REPLICA_VERSION",
    "BG_PROCESS",
    "WRITER_CREATED",
    "WRITER_CLOSED",
    "-SKIPPED-",
    "-SKIPPED-",
    "SCOPE",
    "DF",
    "OWNER",

    // number of properties
    "_COUNT_"
};


IMPLEMENT_ENUM(byte, TickStreamPropertyId, false)
{}

