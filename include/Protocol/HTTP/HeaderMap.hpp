#ifndef hpp_HeaderMap_hpp
#define hpp_HeaderMap_hpp

#include <initializer_list>

// We need a string-view like class for avoiding useless copy here
#include "Strings/ROString.hpp"
// We need intToStr method too
#include "Strings/RWString.hpp"
// We need methods too
#include "Methods.hpp"
// We need TmpString too to persist string and other dynamically sized content to a client's receive buffer
#include "Container/TmpString.hpp"

namespace Protocol::HTTP
{
    enum ParsingError
    {
        InvalidRequest = -1,
        EndOfRequest = 0,
        MoreData = 1,
    };

    using Container::MaxPersistStringArray;

    /** This interface is implemented in structure that take a reference on a temporary buffer and that need to save it to a persistant buffer */
    struct PersistantTag{};
    template <typename Client>
    struct PersistBase : public PersistantTag
    {
        template <std::size_t N>
        inline bool persist(Container::TranscientVault<N> & buffer, std::size_t futureDrop = 0) { return static_cast<Client*>(this)->persist(buffer, futureDrop); }
    };


    namespace HeaderMap
    {
        template <Headers> struct ValueMap;

        // RAII to simplify later code
        struct SetOnExit
        {
            SetOnExit(std::size_t & s, std::size_t value) : v(value), s(s) {}
            ~SetOnExit() { s = v; }
            std::size_t v, & s;
        };
        #define WriteCheck(b, s, v) Protocol::HTTP::HeaderMap::SetOnExit __s(s, v); if (!b || s < v) return true;

        template <typename Child>
        struct LowLevelAccess
        {
            bool getDataPtr(void *& buffer, std::size_t & size) { size = sizeof(static_cast<Child*>(this)->value); buffer = &static_cast<Child*>(this)->value; return true; }
            constexpr static std::size_t getDataSize() { return sizeof(Child::value); }
        };

        /** Link a header with its type (with serializing function for both type) */
        struct ValueBase
        {
            virtual ParsingError parseFrom(ROString & value) = 0;
            virtual bool write(char * buffer, std::size_t & size) const = 0;
            template <Headers h, typename T> T * as() { if constexpr(std::is_same_v<typename ValueMap<h>::ExpectedType, T>) { return static_cast<typename ValueMap<h>::ExpectedType*>(this); } else return (void*)0; }
            virtual void getStringToPersist(MaxPersistStringArray & arr) {  }
        };

        /** String value (opaque) */
        struct StringValue : public ValueBase, public PersistBase<StringValue>, public LowLevelAccess<StringValue>
        {
            typedef ROString ValueType;

            ROString value;
            ParsingError parseFrom(ROString & val) {
                value = val.Trim(' ');
                return EndOfRequest;
            }
            bool write(char * buffer, std::size_t & size) const
            {
                WriteCheck(buffer, size, value.getLength());
                memcpy(buffer, value.getData(), value.getLength());
                return true;
            }
            public: template <typename T> inline bool persist(T & buffer, std::size_t futureDrop = 0) { return Container::persistString(value, buffer, futureDrop); }

            void getStringToPersist(MaxPersistStringArray & arr) { arr[0] = &value; }
            void setValue(const ROString & v) { value = v; }
        };
        template <Headers> struct ValueMap { typedef StringValue ExpectedType; };

        /** Key value in the form "name=value" */
        struct KeyValue : public StringValue
        {
            ROString findValueFor(const ROString & key)
            {
                ROString v = value.fromFirst(key).trimLeft(' ');
                if (v[0] != '=') return ROString();
                return v.trimmedLeft("= ").upToFirst(";").trimRight(' ');
            }
        };

        /** Unsigned integer value (opaque) */
        struct UnsignedValue : public ValueBase, public LowLevelAccess<UnsignedValue>
        {
            typedef size_t ValueType;

            size_t value;
            virtual ParsingError parseFrom(ROString & val) {
                value = (size_t)val.Trim(' ');
                return EndOfRequest;
            }
            bool write(char * buffer, std::size_t & size) const
            {
                char buf[sizeof("18446744073709551615")] = { };
                intToStr((int)value, buf, 10);
                std::size_t s = strlen(buf);
                WriteCheck(buffer, size, s);
                memcpy(buffer, buf, s);
                return true;
            }
            void setValue(const size_t v) { value = v; }
        };

        /** Simple enum value for the given type */
        template <typename Enum, bool strict = false> struct EnumValue : public ValueBase, public LowLevelAccess<EnumValue<Enum, strict>>
        {
            typedef Enum ValueType;

            Enum value;
            virtual ParsingError parseFrom(ROString & val) {
                value = Refl::fromString<Enum>(val.Trim(' ')).orElse(static_cast<Enum>(-1));
                // If we find some unknown value, we don't return an error here, simply continue parsing
                return value != (static_cast<Enum>(-1)) ? EndOfRequest : (strict ? InvalidRequest : EndOfRequest);
            }
            bool write(char * buffer, std::size_t & size) const
            {
                ROString v = Refl::toString(value);
                WriteCheck(buffer, size, v.getLength());
                memcpy(buffer, v.getData(), v.getLength());
                return true;
            }
            void setValue(const Enum v) { value = v; }
        };
        /** Simple enum value for the given type */
        template <typename Enum> struct StrictEnumValue : public EnumValue<Enum, true> {};

        struct EnumValueWithToken
        {
            /** Extract the enum and token value from the given input value */
            static ParsingError parseFrom(ROString & val, ROString & e, ROString & token)
            {
                size_t p = val.findAnyChar(";,", 0, 2);
                if (p != val.getLength() && val[p] == ';') {
                    e = val.splitAt(p).Trim(' ');
                    p = val.findAnyChar(",", 0, 1);
                    token = val.splitAt(p).Trim(' ');
                    val = val.trimLeft(',');
                    return val ? MoreData : EndOfRequest;
                }
                e = val.splitAt(p).Trim(' ');
                val = val.trimLeft(',');
                token = ROString();
                return val ? MoreData : EndOfRequest;
            }
        };
        /** Enum value with quality factor ";q=[.0-9]+,token="
            The quality factor is ignored and so is any token */
        template <typename Enum> struct EnumValueToken : public ValueBase, public LowLevelAccess<EnumValueToken<Enum>>
        {
            typedef Enum ValueType;
            Enum value;
            virtual ParsingError parseFrom(ROString & val)
            {
                ROString v, t;
                ParsingError err = EnumValueWithToken::parseFrom(val, v, t);
                if (err == InvalidRequest) return err;
                value = Refl::fromString<Enum>(v).orElse(static_cast<Enum>(-1));
                return err;
            }
            bool write(char * buffer, std::size_t & size) const
            {
                ROString v = Refl::toString(value);
                WriteCheck(buffer, size, v.getLength());
                memcpy(buffer, v.getData(), v.getLength());
                return true;
            }
            void setValue(const Enum v) { value = v; }
        };

#pragma pack(push, 1)
        /** Enum value that stores the key and value after ';' and before '=' */
        template <typename Enum> struct EnumKeyValue : public ValueBase, public PersistBase<EnumKeyValue<Enum>>
        {
            typedef Enum ValueType;
            Enum value;
            ROString attributes;
            virtual ParsingError parseFrom(ROString & val)
            {
                ROString v;
                ParsingError err = EnumValueWithToken::parseFrom(val, v, attributes);
                if (err == InvalidRequest) return err;
                // Fix case with value containing the attribute itself
                if (!attributes) { attributes = v; v = attributes.splitUpTo("="); }
                value = Refl::fromString<Enum>(v).orElse(static_cast<Enum>(-1));
                return err;
            }
            bool write(char * buffer, std::size_t & size) const
            {
                ROString v = Refl::toString(value);
                WriteCheck(buffer, size, v.getLength() + (attributes.getLength() ? 1 + attributes.getLength() : 0));
                memcpy(buffer, v.getData(), v.getLength());
                if (attributes.getLength()) {
                    memcpy(buffer + v.getLength(), "=", 1);
                    memcpy(buffer + v.getLength() + 1, attributes.getData(), attributes.getLength());
                }
                return true;
            }


            ROString findAttributeValueFor(const ROString & key)
            {
                ROString v = attributes.fromFirst(key).trimLeft(' ');
                if (v[0] != '=') return ROString();
                return v.trimmedLeft("= ").upToFirst(";").trimRight(' ');
            }
            public: template <typename T> inline bool persist(T & buffer, std::size_t futureDrop = 0) { return Container::persistString(attributes, buffer, futureDrop); }
            void getStringToPersist(MaxPersistStringArray & arr) { arr[0] = &attributes; }

            void setValue(Enum v) { value = v; }
            void setValue(Enum v, const ROString & attr) { value = std::forward<Enum>(v); attributes = attr; }

            bool getDataPtr(void *& buffer, std::size_t & size)
            {
                size = sizeof(value) + sizeof(attributes);
                buffer = &value; // Expecting packed structure here, so saving both object at once
                return true;
            }
            static constexpr std::size_t getDataSize() { return sizeof(value) + sizeof(attributes); }
        };

        template <typename E, size_t _N, bool strict = false>
        struct ValueList : public ValueBase, public PersistBase<ValueList<E, _N, strict>>
        {
            E    value[_N];
            uint8 count = 0; // Use a uint8 to avoid a dangling pointer in getDataPtr
            static constexpr uint8 N = (uint8)_N;
            virtual ParsingError parseFrom(ROString & val) {
                ParsingError err;
                for(count = 0; count < N;) {
                    err = value[count++].parseFrom(val);
                    if (err == InvalidRequest) { count--; return err; }
                    if (err == EndOfRequest) return EndOfRequest;
                }
                return count < N ? MoreData : (strict ? InvalidRequest : MoreData); // Is there too much allowed element and we don't support them?
            }
            bool write(char * buffer, std::size_t & size) const
            {
                if (!count) { size = 0; return true; }
                size_t s = 0, vs = 0;
                for (uint8 i = 0; i < count; i++)
                {
                    if (!value[i].write(0, vs)) return false;
                    s += vs + 1;
                }

                WriteCheck(buffer, size, s - 1);
                for (uint8 i = 0; i < count; i++) {
                    vs = size;
                    if (!value[i].write(buffer, vs)) return false;
                    if (i < count - 1) memcpy(buffer + vs, ",", 1);
                    buffer += vs + 1;
                }
                return true;
            }
            public: template <typename T> inline bool persist(T & buffer, std::size_t futureDrop = 0) {
                for (uint8 i = 0; i < count; i++)
                    if (!value[i].persist(buffer, futureDrop)) return false;
                return true;
            }

            void getStringToPersist(MaxPersistStringArray & arr) {
                static_assert(sizeof(arr)/sizeof(arr[0]) >= N && "Please increase the MaxPersistStringArray size if you've increased the ValueList size");
                for (uint8 i = 1; i < count; i++) {
                    value[i].getStringToPersist(arr); arr[i] = arr[0];
                }
                value[0].getStringToPersist(arr);
            }

            void setValue(const E v, const size_t pos = 0) { if (pos < N) { value[pos] = v; if (pos > count) count = (uint8)pos; } }

            template <typename ... U>
            requires ((std::is_same_v<std::decay_t<U>, typename E::ValueType> && ...))
            void setValue(U && ... values) {
                static_assert(sizeof...(values) <= _N && "The given parameter list is larger than the array");
                [&]<std::size_t... Is>(std::index_sequence<Is...>)  {
                    return ((value[Is].setValue(std::forward<U>(values)), true) && ...);
                }(std::make_index_sequence<sizeof...(values)>{});
                count = (uint8)sizeof...(values);
            }

            template <std::size_t M>
            requires (M <= N)
            void setValue(E (&arr)[M]) {
                for (std::size_t i = 0; i < M; i++) value[i] = arr[i];
                count = (uint8)M;
            }

            void setValue(std::initializer_list<typename E::ValueType> && il) {
                auto e = il.begin();
                for (std::size_t i = 0; i < std::min(il.size(), _N); i++) {
                    value[i].setValue(*e);
                    ++e;
                    count = (uint8)(i+1);
                }
            }

            bool getDataPtr(void *& buffer, std::size_t & size)
            {   // Can only answer to query the required size here, we can't reload here here
                size = sizeof(count);
                buffer = &count;
                return false;
            }
            static constexpr std::size_t getDataSize() { return sizeof(uint8) + E::getDataSize() * N; }
        };
#pragma pack(pop)


        template <> struct ValueMap<Headers::Accept>            { typedef ValueList<EnumValueToken<MIMEType>, 16, true> ExpectedType; };
        template <> struct ValueMap<Headers::AcceptCharset>     { typedef ValueList<EnumValueToken<Charset>, 4> ExpectedType; };
        template <> struct ValueMap<Headers::AcceptEncoding>    { typedef ValueList<EnumValueToken<Encoding>, 4> ExpectedType; };
        template <> struct ValueMap<Headers::AcceptLanguage>    { typedef ValueList<EnumKeyValue<Language>, 8> ExpectedType; };
        template <> struct ValueMap<Headers::ContentLanguage>   { typedef ValueList<EnumKeyValue<Language>, 8> ExpectedType; };
        template <> struct ValueMap<Headers::Authorization>     { typedef StringValue ExpectedType; };
        template <> struct ValueMap<Headers::CacheControl>      { typedef ValueList<EnumKeyValue<CacheControl>, 4> ExpectedType; };
        template <> struct ValueMap<Headers::Connection>        { typedef StrictEnumValue<Connection> ExpectedType; };
        template <> struct ValueMap<Headers::ContentEncoding>   { typedef ValueList<EnumValueToken<Encoding>, 2> ExpectedType; };
        template <> struct ValueMap<Headers::ContentType>       { typedef EnumKeyValue<MIMEType> ExpectedType; };
        template <> struct ValueMap<Headers::ContentLength>     { typedef UnsignedValue ExpectedType; };
        template <> struct ValueMap<Headers::Cookie>            { typedef KeyValue ExpectedType; };
        template <> struct ValueMap<Headers::Date>              { typedef StringValue ExpectedType; };
        template <> struct ValueMap<Headers::Host>              { typedef StringValue ExpectedType; };
        template <> struct ValueMap<Headers::Origin>            { typedef StringValue ExpectedType; };
        template <> struct ValueMap<Headers::Range>             { typedef KeyValue ExpectedType; };
        template <> struct ValueMap<Headers::Referer>           { typedef StringValue ExpectedType; };
        template <> struct ValueMap<Headers::TE>                { typedef ValueList<EnumValueToken<Encoding>, 4> ExpectedType; };
        template <> struct ValueMap<Headers::TransferEncoding>  { typedef ValueList<EnumValueToken<Encoding>, 4> ExpectedType; };
        template <> struct ValueMap<Headers::Upgrade>           { typedef StringValue ExpectedType; };
        template <> struct ValueMap<Headers::UserAgent>         { typedef StringValue ExpectedType; };

    }
}


#endif
