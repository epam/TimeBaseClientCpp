#pragma once

#include <dxapi/dxcommon.h>

namespace DxApiImpl {

    struct QqlTokenTypeEnum {
        enum Enum {
            KEYWORD,
            IDENTIFIER,
            FLOAT_LITERAL,
            INT_LITERAL,
            STRING_LITERAL,
            PUNCTUATION,
            INVALID = 0xFF
        };
    };

    ENUM_CLASS(uint8_t, QqlTokenType);

    struct QqlState {
        struct Range {
            int64_t value;

            INLINE bool operator==(const int64_t &other) const { return this->value == other; }
            INLINE bool operator==(const Range &other) const { return this->value == other.value; }

            INLINE int32_t start() const { return value >> 32; }
            INLINE int32_t end() const { return (int32_t)value; }
            INLINE int32_t length() const { return (int32_t)value - (int32_t)(value >> 32); }

            INLINE void set(int start, int end) { value = ((int64_t)start << 32) + end; }
            INLINE static Range make(int start, int end) { Range r; r.set(start, end); return r; }
        };

        struct Token {
            QqlTokenType type;
            Range location;   // High part - start, low part - end
            Token() : type(QqlTokenTypeEnum::INVALID) { location.value = -1; }
        };

        DxApi::Nullable<std::vector<Token>> tokens;
        Range errorLocation;  // High part - start, low part - end
    };
}