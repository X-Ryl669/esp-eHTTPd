#ifndef hpp_Forms_hpp
#define hpp_Forms_hpp

// We need strings
#include "Strings/ROString.hpp"
#include "Strings/CTString.hpp"
// We need URL decode code too
#include "Path/Normalization.hpp"

namespace Network::Servers::HTTP
{
    /** Store the result of a form that's was posted.

        This is used like this:
        @code
        // In your route's callback function:
        FormPost<"name", "value"> form;
        if (!client.fetchContent(headers, form))
        {
            client.closeWithError(Code::BadRequest);
            return true;
        }

        ROString name = form.getValue("name"), value = form.getValue("value");
        @endcode

        The keys for each form encoded pair is usually known beforehand, so it can be set at compile time.
        This thus only stores the values in the object, but the literal ("name" or "value" in the previous example)
        is still stored in the binary since it must be compared at runtime.
        @sa HashFormPost */
    template <CompileTime::str ... keys>
    struct FormPost
    {
        typedef int IsAFormPost; // Simpler to require a member than a template in constexpr expression later on

        /** Where the found values are stored */
        ROString values[sizeof...(keys)];

        static constexpr std::size_t findKeyPos(const ROString key)
        {
            std::size_t pos = 0;
            [&]<std::size_t... Is>(std::index_sequence<Is...>)  {
                return ((key == keys ? false : ++pos) && ...);
            }(std::make_index_sequence<sizeof...(keys)>{});
            return pos;
        }

        static constexpr std::size_t keysCount() { return sizeof...(keys); }

        /** Get the value for the given key */
        ROString getValue(const ROString key)
        {
            std::size_t pos = findKeyPos(key);
            if (pos == keysCount()) return ROString();
            return values[pos];
        }

        /** Parse the values from the keys and the given buffer */
        void parse(ROString buffer)
        {
            // Escape all URL encoded char here
            buffer = Path::URLDecode(buffer);
            while (buffer)
            {
                ROString key = buffer.splitUpTo("=");
                if (key) {
                    std::size_t p = findKeyPos(key);
                    if (p == keysCount()) (void)buffer.splitUpTo("&");
                    else values[p] = buffer.splitUpTo("&");
                }
            }
        }
    };

    /** Store the result of a form that's was posted

        This is used like this:
        @code
        // In your route's callback function:
        HashFormPost<"name"_hash, "value"_hash> form;
        if (!client.fetchContent(headers, form))
        {
            client.closeWithError(Code::BadRequest);
            return true;
        }

        ROString name = form.getValue<"name"_hash>(), value = form.getValue<"value"_hash>();
        @endcode

        The keys for each form encoded pair is usually known beforehand, so it can be set at compile time.
        This thus only stores the values in the object.
        The keys are hashed and only the hash is stored in the binary, which makes both the key/value matching
        faster and smaller in the binary size. However, the hash takes a (32 or 64) bits so it might be larger using the plain
        literal for the key if the key is smaller than 3 chars.
        @sa FormPost */
    template <unsigned ... keysHash>
    struct HashFormPost
    {
        typedef int IsAFormPost; // Simpler to "requires" this member than a complex template in constexpr expression later on

        /** Where the found values are stored */
        ROString values[sizeof...(keysHash)];

        static constexpr std::size_t findKeyPos(const size_t keyHash)
        {
            std::size_t pos = 0;
            [&]<std::size_t... Is>(std::index_sequence<Is...>)  {
                return ((keyHash == keysHash ? false : ++pos) && ...);
            }(std::make_index_sequence<sizeof...(keysHash)>{});
            return pos;
        }

        static constexpr std::size_t keysCount() { return sizeof...(keysHash); }

        /** Get the value for the given key */
        ROString getValue(const ROString key) { return getValue(CompileTime::constHash(key.getData(), key.getLength())); }

        /** Get the value for the given key */
        ROString getValue(const unsigned key)
        {
            std::size_t pos = findKeyPos(key);
            if (pos == keysCount()) return ROString();
            return values[pos];
        }
        /** Compile time version (even faster, since position is computed at compile time) */
        template <unsigned hash>
        ROString getValue() {
            constexpr std::size_t pos = findKeyPos(hash);
            if (pos == keysCount()) return ROString();
            return values[pos];
        }

        /** Parse the values from the keys and the given buffer */
        void parse(ROString buffer)
        {
            // Escape all URL encoded char here
            buffer = Path::URLDecode(buffer);
            while (buffer)
            {
                ROString key = buffer.splitUpTo("=");
                if (key) {
                    std::size_t p = findKeyPos(CompileTime::constHash(key.getData(), key.getLength()));
                    if (p == keysCount()) (void)buffer.splitUpTo("&");
                    else values[p] = buffer.splitUpTo("&");
                }
            }
        }
    };

}

#endif
