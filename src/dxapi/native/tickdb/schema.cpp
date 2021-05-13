#include "tickdb/common.h"
#include "dxapi/schema.h"


// WIP schema verification code. Will probably be redone later

using namespace std;
using namespace DxApiImpl;
using namespace Schema;


const char * infoFieldType[33] = {
    "UNKNOWN",
    "INTEGER-INT8",
    "INTEGER-INT16",
    "INTEGER-INT32",
    "INTEGER-INT48",
    "INTEGER-INT64",
    "boolean",                    // BOOL
    "char",
    "timeOfDay",                  // TIME_OF_DAY
    "FLOAT-IEEE32",               // FLOAT32
    "FLOAT-IEEE64",               // FLOAT64
    "FLOAT-DECIMAL",              // DECIMAL
    "VARCHAR-ALPHANUMERIC(10)",   // ALPHANUMERIC // TODO: KLUDGE
    "VARCHAR-UTF8",               // STRING
    "INTEGER-PUINT30",
    "INTEGER-PUINT61",
    "INTEGER-PINTERVAL",
    "ENUM8",
    "ENUM16",
    "ENUM32",
    "ENUM64",
    "object",
    "array",
    "BINARY",
    "PTIME",
    "25",
    "26",
    "27",
    "28",
    "29",
    "30",
    "31",
    "NULLABLE"
};


#if 0
static struct FieldTypeEnumHelper : EnumHelper<uint8_t,  FieldType::Enum, COUNTOF(infoFieldType), infoFieldType, false> { FieldTypeEnumHelper(); }
___g_enumHelperFieldType;
template<>  EnumClass<FieldTypeEnum>::EnumClass(const char s[]) : value_(___g_enumHelperFieldType.fromString(s)) {}
template<>  EnumClass<FieldTypeEnum>  EnumClass<FieldTypeEnum>::fromChar(int x) { return ___g_enumHelperFieldType.fromChar(x); }
template<> char  EnumClass<FieldTypeEnum>::toChar() const { return ___g_enumHelperFieldType.toChar(*this); }
const char* FieldType::toString() const { return ___g_enumHelperFieldType.toString(*this); }

FieldTypeEnumHelper::FieldTypeEnumHelper()
{}
#endif

#if 1
IMPLEMENT_ENUM(uint8_t, FieldType, false)
{}
#endif


string FieldTypeDescriptor::toString() const
{
    string out;
    uint_fast32_t x = value_;
    if (x & (uint_fast32_t)FieldType::NULLABLE) {
        out.append("NULLABLE_");
    }

    auto t = from(x).type();
    out.append(t.toString());
    switch (t) {
        case FieldType::ALPHANUM:
        case FieldType::DECIMAL_FIXED:
        {
            char tmp[32];
            sprintf(tmp, "%u", from(x).size());
            out.append(tmp);
        }

        default:
            ;
    }

    return out;
}