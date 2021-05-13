#pragma once
#include "dxplatform.h"
#include <stdexcept>

// "Disallow evil constructors" macros

#define DISALLOW_COPY_AND_ASSIGN(TypeName) private:                 \
    TypeName(const TypeName&) = delete;                             \
    void operator=(const TypeName&) = delete


#define DISALLOW_DEFAULT_CONSTRUCTORS(TypeName) private:            \
    TypeName() = delete;                                            \
    TypeName(const TypeName&) = delete;                             \
    void operator=(const TypeName&) = delete


namespace DxApi {
    template<typename T> class Nullable {
    public:
        INLINE Nullable() : value_(), isSet_(false) {}
        INLINE Nullable(T value) : value_(value), isSet_(true) {}

        INLINE Nullable(const Nullable& other) : value_(other.value_), isSet_(other.isSet_) {}

        INLINE friend void swap(Nullable& a, Nullable& b)
        {
            using std::swap;
            swap(a.value_, b.value_);
            swap(a.isSet_, b.isSet_);
        }

        INLINE const Nullable<T>& operator=(const Nullable<T> &other)
        {
            return set(other.value_, other.isSet_);
        }


        INLINE const Nullable<T>& operator=(const T &value)
        {
            return set(value, true);
        }


        INLINE const Nullable<T>& operator=(std::nullptr_t other)
        {
            return set(T(), false);
        }


        INLINE bool operator==(const T &value) const
        {
            return isSet_ && value_ == value;
        }


        INLINE bool operator==(const Nullable<T>& other) const
        {
            return isSet_ == other.isSet_ && (!isSet_ || value_ == other.value_);
        }


        INLINE const T& get() const
        {
            if (isSet_)
                return value_;
            else
                throw std::logic_error("Attempting to get value of empty Nullable");
        }


        INLINE T& get()
        {
            if (isSet_)
                return value_;
            else
                throw std::logic_error("Attempting to get value of empty Nullable");
        }


        INLINE bool is_set()    const { return  isSet_; }
        INLINE bool has_value() const { return  isSet_; }
        INLINE bool is_null()   const { return !isSet_; }

        INLINE void reset() { set(T(), false); }

    private:
        INLINE const Nullable<T>& set(const T &value, bool isSet = true) { value_ = value; isSet_ = isSet; return *this; }

    private:
        T value_;
        bool isSet_;
    };
}


namespace DxApiImpl {

    //// This template is used for DxApi constant enumeration types
    template<typename INTTYPE, class ENUM> class EnumClass : public ENUM {
    public:

        // Return underlying integer type
        INLINE INTTYPE toInt()  const { return value_; }

        // Convert to enum type
        INLINE operator typename EnumClass::Enum()          const { return (typename EnumClass::Enum)value_; }

        // Convert to constant string
        //const char * toString() const;

        /*INLINE friend std::ostream& operator<<(std::ostream& os, const EnumClass &me)
        {
            os << me.toString();
            return os;
        }*/

        // Convert to single char representation. Internal use
        char toChar()           const;

        // Convert from integer. Will throw generic exception if out of range
        //INLINE static EnumClass from(INTTYPE x)                   { return (typename EnumClass::Enum)x; }

        // Convert from integer. Will throw generic exception if out of range
        //INLINE static EnumClass fromInt(int x)                    { return (typename EnumClass::Enum)x; }

        // Convert from string. Should match the enum name, case-sensitive. Will throw generic exception if out of range. 
        //INLINE static EnumClass fromString(const char x[])        { return EnumClass(x); }

        // Convert from string. Should match the enum name, case-sensitive. Will throw generic exception if out of range
        //INLINE static EnumClass fromString(const std::string &x)  { return EnumClass(x.c_str()); }

        // Convert from single-character representation. Internal use
        //static EnumClass fromChar(int x);


        // Copy constructor
        INLINE EnumClass(const EnumClass &other) : value_(other.value_)  {}

        // Create from enum member
        //INLINE EnumClass(typename ENUM::Enum other) : value_(other)  {}
        INLINE EnumClass(typename EnumClass::Enum other) : value_(other)  {}

        // Create from string, same behavior as fromString
        //EnumClass(const char value[]);

    protected:
        // Default constructor is disabled, because there is no predefined default value for this template class
        EnumClass() = delete;

        // Holds actual value
        INTTYPE value_;
    };



// 
/**
 * This macro lets you declare DxApi enum class while being outside of DxApiImpl namespace
 * NOTE: For Linux port this implementation had to be made due to C++ standard rules(ignored by MSVC) preventing the previous one from working.
 *       Basically, all non-trivial methods are redefined in this macro. Will be replaced with better solution, when/if it is found.
 */
#define ENUM_CLASS(INTTYPE, ENUM)                                                                       \
    class ENUM : public DxApiImpl::EnumClass<INTTYPE, ENUM##Enum> {                                     \
    public:                                                                                             \
        char toChar()           const;                                                                  \
        const char* toString() const;                                                                  \
        INLINE friend std::ostream& operator<<(std::ostream& os, const ENUM &me)                        \
        {                                                                                               \
            os << std::string(me.toString()); /* TODO: temp fix for clang linux compiler */             \
            return os;                                                                                  \
        }                                                                                               \
                                                                                                        \
        static ENUM fromChar(int x);                                                                    \
        INLINE static ENUM fromString(const char x[])        { return ENUM(x); }                        \
        INLINE static ENUM fromString(const std::string &x)  { return ENUM(x.c_str()); }                \
        INLINE static ENUM from(INTTYPE x)                   { return (ENUM::Enum)x; }                  \
        INLINE static ENUM fromInt(int x)                    { return (ENUM::Enum)x; }                  \
        ENUM(const char value[]);                                                                       \
        INLINE ENUM(const DxApiImpl::EnumClass<INTTYPE, ENUM##Enum> &o) : DxApiImpl::EnumClass<INTTYPE, ENUM##Enum>(o) {}          \
        INLINE ENUM(const DxApiImpl::EnumClass<INTTYPE, ENUM##Enum>::Enum o) : DxApiImpl::EnumClass<INTTYPE, ENUM##Enum>(o) {}     \
    };


 // TODO: Cleanup

    //const { return EnumClass<T##Enum>::toString(); }

    /*namespace __dummy##__LINE__ { ::DxApiImpl::EnumClass<T##Enum> _dummy(0); }*/ 

   //     extern template class ::DxApiImpl::EnumClass<T##Enum>;

   
#if 0
#define ENUM_CLASS(T) \
    class T : public DxApiImpl::EnumClass<T##Enum> {}

#endif
}