#ifndef hpp_HeadersArray_hpp
#define hpp_HeadersArray_hpp

// We need methods and request line parsing
#include "Protocol/HTTP/RequestLine.hpp"
// We need compile time vectors here to cast some magical spells on types
#include "Container/CTVector.hpp"
#include "Container/RingBuffer.hpp"

namespace Network::Common::HTTP
{
    using namespace Protocol::HTTP;

    namespace Details
    {
        // This works when they are all the same type
        template<typename Result, typename... Ts>
        Result& runtime_get(std::size_t i, std::tuple<Ts...>& t)  {
            return [&]<std::size_t... Is>(std::index_sequence<Is...>)  {
                return std::array<std::reference_wrapper<Result>, sizeof...(Ts)>{ (Result&)std::get<Is>(t)... }[i];
            }(std::index_sequence_for<Ts...>());
        }

        // Convert a std::array of headers to a parametric typelist
        template <Headers E> struct MakeRequest { typedef Protocol::HTTP::RequestHeader<E> Type; };
        // Convert a std::array of headers to a parametric answer list
        template <Headers E> struct MakeAnswer { typedef Protocol::HTTP::AnswerHeader<E> Type; };
    }

    // Useless vomit of useless garbage to get the inner content of a typelist
    template <auto headerArray, typename U> struct HeadersArray {};
    // Useless vomit of useless garbage to get the inner content of a typelist
    template <auto headerArray, typename U> struct AnswerHeadersArray {};
    /** The headers array with a compile time defined heterogenous tuple of headers */
    template <auto headerArray, typename ... Header>
    struct HeadersArray<headerArray, Container::TypeList<Header...>>
    {
        std::tuple<Header...> headers;

        // Runtime version, slower O(N) at runtime, and takes more binary space since it must store an array of values here
        RequestHeaderBase * getHeader(const Headers h)
        {
            // Need to find the position in the tuple first
            std::size_t pos = 0;
            for(; pos < headerArray.size(); ++pos) if (headerArray[pos] == h) break;
            if (pos == headerArray.size()) return 0; // Not found, don't waste time searching for it

            return &Details::runtime_get<RequestHeaderBase>(pos, headers);
        }

        static constexpr std::size_t findHeaderPos(const Headers h) {
            std::size_t pos = 0;
            for(; pos < headerArray.size(); ++pos) if (headerArray[pos] == h) break;
            return pos;
        }
        // Compile time version, faster O(1) at runtime, and smaller, obviously
        template <Headers h>
        RequestHeader<h> & getHeader()
        {
            constexpr std::size_t pos = findHeaderPos(h);
            if constexpr (pos == headerArray.size())
            {   // If the compiler stops here, you're querying a header that doesn't exist...
                throw "Invalid header given for this type, it doesn't contain any";
//                    static RequestHeader<h> invalid;
//                    return invalid;
            }
            return std::get<pos>(headers);
        }
        // Compile time version, faster O(1) at runtime, and smaller, obviously
        template <Headers h>
        const RequestHeader<h> & getHeader() const
        {
            constexpr std::size_t pos = findHeaderPos(h);
            if constexpr (pos == headerArray.size())
            {   // If the compiler stops here, you're querying a header that doesn't exist...
                throw "Invalid header given for this type, it doesn't contain any";
//                    static RequestHeader<h> invalid;
//                    return invalid;
            }
            return std::get<pos>(headers);
        }

        // Runtime version to test if we are interested in a specific header (speed up O(M*N) search to O(m*N) search instead)
        Headers acceptHeader(const ROString & header)
        {
            // Only search for headers we are interested in
            for(std::size_t pos = 0; pos < headerArray.size(); ++pos)
                if (header == Refl::toString(headerArray[pos])) return headerArray[pos];

            return Headers::Invalid;
        }

        // Runtime version to accept header and parse the value in the expected element
        ParsingError acceptAndParse(const ROString & header, ROString & input)
        {
            ParsingError err = InvalidRequest;
            [&]<std::size_t... Is>(std::index_sequence<Is...>)  {
                return ((header == Refl::toString(headerArray[Is]) ? (err = std::get<Is>(headers).acceptValue(input), true) : false) || ...);
            }(std::make_index_sequence<sizeof...(Header)>{});
            return err;
        }

        constexpr std::size_t getRequiredVaultSize()
        {
            return [&]<std::size_t... Is>(std::index_sequence<Is...>)  {
                return (std::get<Is>(headers).parsed.getDataSize() + ...);
            }(std::make_index_sequence<sizeof...(Header)>{});
        }

        static bool saveBuf(void * src, void * dest, std::size_t s, uint8 *& buf, std::size_t & size)
        {
            if (s > size) return false;
            memcpy(dest, src, s);
            buf += s;
            size -= s;
            return true;
        }

        template <typename T>
        static bool serializeHeaderToBuffer(T & t, uint8 *& buf, std::size_t & size, void * b, bool direction)
        {
            std::size_t s = 0;
            void *& src = direction ? b : (void*&)buf, *& dest = direction ? (void*&)buf : b;
            if (t.getDataPtr(b, s))
                return saveBuf(src, dest, s, buf, size);

            if constexpr(requires { t.count; }) {
                if (!saveBuf(src, dest, s, buf, size)) return false;
                for (uint8 i = 0; i < t.count; i++)
                {
                    t.value[i].getDataPtr(b, s);
                    if (!saveBuf(src, dest, s, buf, size)) return false;
                }
                return true;
            } else return false;
        }

        template <typename T>
        bool saveHeaderToBuffer(T & t, uint8 *& buf, std::size_t & size) { void * b = 0; return serializeHeaderToBuffer(t, buf, size, b, true); }
        template <typename T>
        bool loadHeaderFromBuffer(T & t, uint8 *& buf, std::size_t & size) { void * b = 0; return serializeHeaderToBuffer(t, buf, size, b, false); }

        template <std::size_t N>
        bool saveInVault(Container::TranscientVault<N> & buffer)
        {
            std::size_t size = getRequiredVaultSize();
            // Save the buffer to the stack before being erased (in reverse order)
            if (uint8 * buf = buffer.reserveInVault(size))
            {
                // Got a buffer, it's time to save all of our stuff in that buffer
                bool ret = [&]<std::size_t... Is>(std::index_sequence<Is...>)  {
                    return (saveHeaderToBuffer(std::get<Is>(headers).parsed, buf, size) && ...);
                }(std::make_index_sequence<sizeof...(Header)>{});
                return ret;
            }
            return false;
        }

        template <std::size_t N>
        bool loadFromVault(Container::TranscientVault<N> & buffer)
        {
            std::size_t size = buffer.vaultSize();
            // Save the buffer to the stack before being erased (in reverse order)
            if (uint8 * buf = buffer.getVaultHead())
            {
                // Got a buffer, it's time to save all of our stuff in that buffer
                bool ret = [&]<std::size_t... Is>(std::index_sequence<Is...>)  {
                    return (loadHeaderFromBuffer(std::get<Is>(headers).parsed, buf, size) && ...);
                }(std::make_index_sequence<sizeof...(Header)>{});
                return ret;
            }
            return false;
        }
    };



    template <auto headerArray, typename ... Header>
    struct AnswerHeadersArray<headerArray, Container::TypeList<Header...>>
    {
        std::tuple<Header...> headers;

        static constexpr std::size_t findHeaderPos(const Headers h) {
            std::size_t pos = 0;
            for(; pos < headerArray.size(); ++pos) if (headerArray[pos] == h) break;
            return pos;
        }
        // Compile time version, faster O(1) at runtime, and smaller, obviously
        template <Headers h>
        AnswerHeader<h> & getHeader()
        {
            constexpr std::size_t pos = findHeaderPos(h);
            if constexpr (pos == headerArray.size())
            {   // If the compiler stops here, you're querying a header that doesn't exist...
                throw "Invalid header given for this type, it doesn't contain any";
//                    static RequestHeader<h> invalid;
//                    return invalid;
            } else return std::get<pos>(headers);
        }

        template <Headers h>
        const AnswerHeader<h> & getHeader() const
        {
            constexpr std::size_t pos = findHeaderPos(h);
            if constexpr (pos == headerArray.size())
            {   // If the compiler stops here, you're querying a header that doesn't exist...
                // The error is probably that you forgot to add this header in the route, or you're using the same
                // callback for a POST and GET method (the former adds ContentLength automatically, but the latter doesn't)
                // In that case, either add the missing header or split in 2 routes for each method
                throw "Invalid header given for this type, it doesn't contain any";
//                    static RequestHeader<h> invalid;
//                    return invalid;
            } else return std::get<pos>(headers);
        }

        template <Headers h>
        bool hasValidHeader() const
        {
            constexpr std::size_t pos = findHeaderPos(h);
            if constexpr (pos == headerArray.size())
            {
                return false;
            } else
            {
                return std::get<pos>(headers).isSet();
            }
        }

        template <Headers h, typename Value>
        bool setHeaderIfUnset(Value && v) const
        {
            constexpr std::size_t pos = findHeaderPos(h);
            if constexpr (pos != headerArray.size())
            {
                if (auto & header = std::get<pos>(headers); header.isSet())
                {
                    header.setValue(std::forward(v));
                    return true;
                }
            }
            return false;
        }

#if MinimizeStackSize == 1
        bool sendHeaders(BaseSocket & socket)
        {
            return [&]<std::size_t... Is>(std::index_sequence<Is...>)  {
                return (std::get<Is>(headers).send(socket) && ...);
            }(std::make_index_sequence<sizeof...(Header)>{});
        }
#else
        bool sendHeaders(Container::TrackedBuffer & buffer)
        {
            return [&]<std::size_t... Is>(std::index_sequence<Is...>)  {
                return (std::get<Is>(headers).write(buffer) && ...);
            }(std::make_index_sequence<sizeof...(Header)>{});
        }
#endif
    };

    /** Convert the list of headers you're expecting to the matching HeadersArray the library is using */
    template <Headers ... allowedHeaders>
    struct ToHeaderArray {
        static constexpr auto headersArray = Container::getUnique<std::array<Headers, sizeof...(allowedHeaders)>{allowedHeaders...}, std::array{Headers::Authorization, Headers::Connection}>();
        typedef HeadersArray<headersArray, decltype(Container::makeTypes<Details::MakeRequest, headersArray>())> Type;
    };

    /** Convert the list of headers you're expecting to the matching HeadersArray the library is using */
    template <Headers ... allowedHeaders>
    struct ToPostHeaderArray {
        static constexpr auto headersArray = Container::getUnique<std::array<Headers, sizeof...(allowedHeaders)>{allowedHeaders...}, std::array{Headers::ContentType, Headers::ContentLength, Headers::Connection}>();
        typedef HeadersArray<headersArray, decltype(Container::makeTypes<Details::MakeRequest, headersArray>())> Type;
    };


    template <MethodsMask mask, Headers ... allowedHeaders>
    struct MakeHeadersArray {
        // Select the best header array implementation here
        static constexpr auto bestHeaderArray() {
            if constexpr (mask.mask & MethodsMask{Method::POST, Method::PUT}.mask)
                return typename ToPostHeaderArray<allowedHeaders...>::Type{};
            else
                return typename ToHeaderArray<allowedHeaders...>::Type{};
        }

        typedef decltype(bestHeaderArray()) Type;
    };


    template <Headers ... answerHeaders>
    struct ToAnswerHeader {
        static constexpr size_t HeaderCount = sizeof...(answerHeaders);
        static constexpr auto headersArray = Container::getUnique<std::array<Headers, sizeof...(answerHeaders)>{answerHeaders...}, std::array{Headers::WWWAuthenticate}>();
        typedef AnswerHeadersArray<headersArray, decltype(Container::makeTypes<Details::MakeAnswer, headersArray>())> Type;
    };
}


#endif
