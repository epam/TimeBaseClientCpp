#include "tickdb/common.h"
#include "qqlstate.h"


using namespace DxApiImpl;

const char * infoQqlTokenType[] = {
    "KEYWORD",
    "IDENTIFIER",
    "FLOAT_LITERAL",
    "INT_LITERAL",
    "STRING_LITERAL",
    "PUNCTUATION"
};

IMPLEMENT_ENUM(uint8_t, QqlTokenType, false)
{}