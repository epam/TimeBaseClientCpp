#include "tickdb/common.h"
#include <dxapi/dxapi.h>


using namespace std;
using namespace DxApi;
using namespace DxApiImpl;
using namespace DxApiImpl::TDB;


// REFERENCE: deltix.qsrv.hf.pub.InstrumentType /deltix/qsrv/hf/pub/InstrumentType.java:8 

const char * infoInstrumentType[] = {
    "S-EQUITY", // Stock
    "O-OPTION",
    "F-FUTURE",
    "B-BOND",
    "X-FX",
    "I-INDEX",
    "E-ETF",
    "C-CUSTOM",
    "P-SIMPLE_OPTION",

    "G-EXCHANGE",
    "T-TRADING_SESSION",
    "M-STREAM",
    "Q-DATA_CONNECTOR",
    "Z-EXCHANGE_TRADED_SYNTHETIC",

    "x-SYSTEM", // TODO: SYSTEM and FX use same char?
    "D-CFD",

    "U-UNDEFINED"   // 
};


IMPLEMENT_ENUM(uint8_t, InstrumentType, true)
{
    // Verify that format did not change
    assert(InstrumentType::EQUITY == fromChar('S'));
    assert(InstrumentType::OPTION == fromChar('O'));
    assert(InstrumentType::FUTURE == fromChar('F'));
    assert(InstrumentType::BOND == fromChar('B'));
    assert(InstrumentType::FX == fromChar('X'));
    assert(InstrumentType::INDEX == fromChar('I'));
    assert(InstrumentType::ETF == fromChar('E'));
    assert(InstrumentType::CUSTOM == fromChar('C'));
    assert(InstrumentType::SIMPLE_OPTION == fromChar('P'));
    assert(InstrumentType::EXCHANGE == fromChar('G'));
    assert(InstrumentType::TRADING_SESSION == fromChar('T'));
    assert(InstrumentType::STREAM == fromChar('M'));
    assert(InstrumentType::DATA_CONNECTOR == fromChar('Q'));
    assert(InstrumentType::EXCHANGE_TRADED_SYNTHETIC == fromChar('Z'));
    assert(InstrumentType::SYSTEM == fromChar('x')); // TODO: character X also matches InstrumentType::FX ???
    assert(InstrumentType::CFD == fromChar('D'));
    
    //assert(InstrumentType::UNDEFINED == fromChar('U')); // TODO: Deal with this check later

    forn(i, COUNTOF(infoInstrumentType)) {
        assert(fromChar(infoInstrumentType[i][0]) == fromString(infoInstrumentType[i] + 2));
    }
    // DBG:
    //puts("222");
    //assert(!"Hello World");
}



// This constructor does not work
//InstrumentType::InstrumentType(const char s[]) : value_(helper.fromString(s)) {}

// This constructor works
//InstrumentType::EnumClass(const char s[]) : value_(helper.fromString(s)) {}
