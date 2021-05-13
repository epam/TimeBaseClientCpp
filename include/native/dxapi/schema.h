#pragma once

#include <dxapi/dxconstants.h>
#include <dxapi/dxcommon.h>

#include <unordered_map>
#include <memory>

namespace Schema {
    struct FieldTypeEnum {
        enum Enum {
            UNKNOWN,    // This value is used as terminator
            INT8,
            INT16,
            INT32,
            INT48,
            INT64,
            BOOL,
            CHAR,
            TIME_OF_DAY,
            FLOAT32,
            FLOAT64,        // 10
            DECIMAL_AUTO,
            DECIMAL_FIXED,
            ALPHANUM,
            STRING,
            PUINT30,
            PUINT61,
            PINTERVAL,
            ENUM8,
            ENUM16,
            ENUM32,         // 20
            ENUM64,
            OBJECT,
            ARRAY,
            BINARY,
            PTIME,          // 25
            NULLABLE    = 0x20,
            INVALID     = 0xFF
        };
    };

    ENUM_CLASS(uint8_t, FieldType);
    
    class FieldTypeDescriptor {
    public:
        FieldType type()  const;
        unsigned size()   const;
        bool isNullable() const;
        std::string toString() const;

        FieldTypeDescriptor(FieldType type, unsigned size, bool isNullable);
        FieldTypeDescriptor(uint32_t v) { value_ = v;  }
        template<typename T> static const FieldTypeDescriptor& from(const T &c);

    protected:
        FieldTypeDescriptor() = default;
        uint32_t value_;
    };


    // Uncomment this and MSVC compiler crashes. TODO: delete
    /*INLINE Schema::FieldType::operator FieldType::Enum()   const { return (Enum)value_; }
    INLINE int FieldType::toInt() const { return value_; }
    INLINE FieldType FieldType::fromInt(int x)                    { return (FieldType::Enum)x; }
    INLINE FieldType FieldType::fromString(const char x[])        { return FieldType(x); }
    INLINE FieldType FieldType::fromString(const std::string &x)  { return FieldType(x.c_str()); }
    INLINE FieldType::FieldType(Enum other) : value_((uint8_t)other)  {}*/


        //INLINE FieldType FieldTypeDescriptor::type()  const { return FieldType((FieldType::Enum)*(uint8_t *)value_); }
    INLINE FieldType FieldTypeDescriptor::type()  const { return FieldType::fromInt(value_ & ((uint32_t)FieldTypeEnum::NULLABLE - 1)); }
    INLINE unsigned FieldTypeDescriptor::size()   const { return (value_ >> 8) & 0xFFFF; }
    INLINE bool FieldTypeDescriptor::isNullable() const { return 0 != (value_ & FieldTypeEnum::NULLABLE); }

    template<typename T> INLINE const FieldTypeDescriptor& FieldTypeDescriptor::from(const T &c) { return DxApiImpl::_as<FieldTypeDescriptor>(c); }

    INLINE FieldTypeDescriptor::FieldTypeDescriptor(FieldType type, unsigned size, bool isNullable) : value_(type.toInt() | ((size & 0xFFFF) << 8) | (isNullable *  FieldTypeEnum::NULLABLE)) {}

    class DataType {
    public:
        FieldTypeDescriptor descriptor;
        std::string typeName;
        std::string encodingName;
        std::string descriptorGuid;
        bool isNullable;

        std::shared_ptr<DataType> elementType;
        std::vector<std::string> types;

        DataType() : descriptor(FieldType::UNKNOWN, 0, false), isNullable(0), elementType(nullptr) { }
    };

    class FieldInfo {
    public:
        std::string name;
        std::string relativeTo;
        DataType dataType;
            
        //void setType(const char * type);
        FieldInfo() { }
    };

    class TickDbClassDescriptor {
    public:
        std::string className;
        std::string guid;
        std::string parentGuid;

        FieldType enumType; // Size of the enum member, if this class describes enum
        std::unordered_map<std::string, int32_t> symbolToEnumValue;
        std::vector<std::string> enumSymbols;

        bool isContentClass;
        std::vector<FieldInfo> fields; // Non-static fields of the class

                                        // Changed parent pointers to array indices to avoid dangling pointers when container vector is reallocated after copying or resizing
                                        // values < 0 are invalid
        intptr_t parentIndex;

    public:
        TickDbClassDescriptor() : enumType(FieldType::UNKNOWN), isContentClass(false), parentIndex(-1) {
        }

        static std::vector<TickDbClassDescriptor> parseDescriptors(const std::string &messageDescriptorSchema, bool parseFields);
    };

    

}