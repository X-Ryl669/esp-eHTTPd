#ifndef hpp_RequestLine_hpp
#define hpp_RequestLine_hpp

// We need our configuration
#include "HTTPDConfig.hpp"
// We need header map, ParsingError and persistence interface
#include "HeaderMap.hpp"
// We need concepts too
#include "Concepts.hpp"


#if defined(MaxSupport)
  // We need path normalization
  #include "Path/Normalization.hpp"
#endif

// The REQUEST line is defined in section 5.1 in RFC2616
namespace Protocol::HTTP
{


    /** A typical Query part in the URL is what follow the question mark in this: "?a=b&c[]=3&c[]=4&d"
        This class allows to locate keys and value and returns them one by one.
        The URI isn't URL decoded, here, it can be done in the RequestURI's function too by using the normalize method */
    struct Query : public PersistBase<Query>
    {
        ROString query;

    public: template <typename T> inline bool persist(T & buffer, std::size_t futureDrop = 0) { return Container::persistString(query, buffer, futureDrop); }
        // Interface
    public:
        /** Get the value for the given key
            @param key      The key to search for
            @param startPos Optional: If provided, skip the given bytes of the query (used as an optimization step)

            This algorithm is O(N*M), with N the query size in byte and M the key size in byte.
            So even with small N or M try to avoid iterating with this, use the next method instead

            @return The value if any match found or empty string else */
        ROString getValueFor(const ROString & key, const size_t startPos = 0)
        {
            ROString candidate = query.midString(startPos, query.getLength()).fromFirst(key);
            while (candidate)
            {
                if (candidate[0] == '=')
                {
                    candidate.splitAt(1);
                    return candidate.splitUpTo("&");
                }
                candidate = candidate.fromFirst(key);
            }
            return ROString();
        }
        /** Iterate the keys in the query
            @param iter  Opaque value used to speed value extraction. Start with 0 here. You can pass iter to getValueFor for faster value extraction.
            @param key   If any key found, this will be stored here
            @param value If any key found, this will store the associated value or empty string for non valued keys
            @return true if iteration was successful, false when ending */
        bool iterateKeys(size_t & iter, ROString & key, ROString & value)
        {
            if (iter >= query.getLength()) return false;
            ROString q = query.midString(iter, query.getLength());
            key = q.splitUpTo("=");
            if (key) {
                iter += key.getLength() + 1;
                value = q.splitUpTo("&");
                iter += value.getLength() + 1;
                return true;
            }
            if (q)
            {
                key = q; value = ROString();
                iter += q.getLength();
                return true;
            }
            return false;
        }
        Query(const ROString & query) : query(query) {}
    };

    /** For the sake of compactness, this isn't a full URI parsing scheme. The request URI for a server that's not a proxy is
        always: (section 5.2.1)
            '*'  (typically for OPTIONS)
         or absolute path (including query parameters if any)
    */
    struct RequestURI  : public PersistBase<RequestURI>
    {
        // Members
    public:
        /** The absolute path given to the URI */
        ROString absolutePath;

    public: template <typename T> inline bool persist(T & buffer, std::size_t futureDrop = 0) { return Container::persistString(absolutePath, buffer, futureDrop); }

        // Interface
    public:
        /** Check if the given request applies to all ressources */
        bool appliesToAllRessources() const { return absolutePath == "*"; }
        /** Check if the given URI is not empty */
        explicit operator bool() const { return absolutePath; }
        /** Get the first query part in the request URI (if any) */
        Query getQueryPart() const { return Query{absolutePath.fromFirst("?")}; }
        /** Get only the path, not the query part */
        ROString onlyPath() const { return absolutePath.upToFirst("?"); }

        RequestURI & operator = (const ROString & path) { absolutePath = path; return *this; }
#if defined(MaxSupport)
        bool normalizePath() { return Path::normalize(absolutePath, true); }
#else
        bool normalizePath() { return true; }
#endif
    };
    /** Request line is defined as METHOD SP Request-URI SP HTTP-Version CRLF */
    struct RequestLine : PersistBase<RequestLine>
    {
        /** The requested method */
        Method      method = Method::Invalid;
        /** The requested URI */
        RequestURI  URI;
        /** The protocol version */
        Version     version = Version::Invalid;

    public: template <typename T> inline bool persist(T & buffer, std::size_t futureDrop = 0) { return URI.persist(buffer, futureDrop); }
        // Interface
    public:
        /** Parse the given data stream */
        ParsingError parse(ROString & input)
        {
            ROString m = input.splitUpTo(" ");
            method = Refl::fromString<Method>(m).orElse(Protocol::HTTP::Method::Invalid);
            if (method == Method::Invalid) return InvalidRequest;

            input = input.trimLeft(' ');
            URI = input.splitUpTo(" ");
            if (!URI || !input) return InvalidRequest;

            input = input.trimLeft(' ');
            if (input.splitUpTo("/1.") != "HTTP") return InvalidRequest;
            if (input[0] > '1' || input[0] < '0') return InvalidRequest;
            version = input[0] == '1' ? Version::HTTP1_1 : Version::HTTP1_0;

            if (input[1] != '\r' || input[2] != '\n') return InvalidRequest;
            (void)input.splitAt(3);
            return MoreData;
        }

        void reset() { method = Method::Invalid; URI = ROString(); version = Version::Invalid; }
    };

    struct GenericHeaderParser
    {
        /** Parse the given data stream until the header is found (stop at value) */
        static ParsingError parseHeader(ROString & input, ROString & header)
        {
            input = input.trimmedLeft();
            if (!input) return EndOfRequest; // End of headers here or error
            header = input.splitUpTo(":").trimRight(' ');
            return MoreData;
        }

        /** Skip value for this header */
        static ParsingError skipValue(ROString & input)
        {
            input.splitUpTo("\r\n");
            return MoreData;
        }

        /** Parse the given data stream until the end of the value, should be at value */
        static ParsingError parseValue(ROString & input, ROString & value)
        {
            input = input.trimLeft(' ');
            if (!input) return InvalidRequest;
            value = input.splitUpTo("\r\n").trimRight(' ');
            return MoreData;
        }
    };

    /** Request header line parsing, as specified in section 5.3 */
    struct GenericRequestHeaderLine : public PersistBase<GenericRequestHeaderLine>
    {
        /** The header name */
        ROString header;
        /** The header value */
        ROString value;

        // Interface
    public:
        /** Parse the given data stream until the header is found (stop at value) */
        ParsingError parseHeader(ROString & input) { return GenericHeaderParser::parseHeader(input, header); }
        /** Skip value for this header */
        ParsingError skipValue(ROString & input) { return GenericHeaderParser::skipValue(input); }
        /** Parse the given data stream */
        ParsingError parse(ROString & input)
        {
            if (ParsingError err = parseHeader(input); err != MoreData) return err;
            return GenericHeaderParser::parseValue(input, value);
        }
        /** Extract header name type
            @return InvalidHeader upon unknown or invalid header */
        Headers getHeaderType() const { return Refl::fromString<Headers>(header).orElse(Headers::Invalid); }

    public: template <typename T> inline bool persist(T & buffer, std::size_t futureDrop = 0) { return Container::persistString(header, buffer, futureDrop) && Container::persistString(value, buffer, futureDrop);  }
    };

    struct RequestHeaderBase
    {
        /** The header value */
        ROString rawValue;

        ParsingError parse(ROString & input)
        {
            ROString header;
            if (ParsingError err = GenericHeaderParser::parseHeader(input, header); err != MoreData) return err;
            if (!acceptHeader(header)) return Protocol::HTTP::GenericHeaderParser::skipValue(input);
            return acceptValue(input, rawValue);
        }

        ParsingError acceptValue(ROString & input) { return acceptValue(input, rawValue); }
        virtual bool acceptHeader(ROString & header) const { return true; }
        virtual ParsingError acceptValue(ROString & input, ROString & value) { return GenericHeaderParser::parseValue(input, value); }
        virtual HeaderMap::ValueBase * getPersistValue(){ return nullptr; }
    };

    struct InvalidRequestHeaderBase : public RequestHeaderBase
    {
        virtual ParsingError acceptValue(ROString & input, ROString & value) { return MoreData; }
    };

    /** Type specified request header line and value */
    template <Headers h>
    struct RequestHeader : public RequestHeaderBase
    {
        typedef typename HeaderMap::ValueMap<h>::ExpectedType ValueType;
        ValueType parsed;
        static constexpr Headers header = h;
        using RequestHeaderBase::acceptValue;

        /** Check to see if this header is the expected type and in that case, capture the value */
        bool acceptHeader(ROString & hdr) const { return hdr == Refl::toString(h); }
        /** Accept the value for this header */
        virtual ParsingError acceptValue(ROString & input, ROString & val) {
            val = input.splitUpTo("\r\n");
            ROString tmp = val;
            val = val.trimRight(' ');
            return parsed.parseFrom(tmp);
        }
        static RequestHeader<h> createFrom(ROString value) { RequestHeader<h> hdr{}; hdr.parsed.parseFrom(value); return hdr; }

        HeaderMap::ValueBase * getPersistValue() {
            if constexpr(std::is_base_of<PersistantTag, std::decay_t<decltype(parsed)>>::value) {
                return &parsed;
            }
            return nullptr;
        }

        /** Get the number of element that were parsed (usually 1) */
        std::size_t getValueElementsCount() const {
            if constexpr(requires{ parsed.count; }) {
                return parsed.count;
            } else { return 1; }
        }
        /** Get the i-th element that was parsed */
        auto getValueElement(std::size_t i) const {
            if constexpr(requires{ parsed.count; }) {
                if (i >= parsed.count)
                    // All enumeration used for headers are made to accept -1 as error
                    return decltype(parsed.value[0].value)(-1);
                return parsed.value[i].value;
            } else return parsed.value;
        }
    };



    template <Headers h>
    struct AnswerHeader
    {
        typename HeaderMap::ValueMap<h>::ExpectedType v;
        static constexpr Headers header = h;

        bool write(char * buffer, std::size_t & size)
        {
            const ROString & s = Refl::toString(header);
            std::size_t vs = 0;
            if (!v.write(0, vs)) return false;
            if (vs == 0) return true; // Skip headers without any value, since it's wrong anyway
            WriteCheck(buffer, size, s.getLength() + vs + 3);
            memcpy(buffer, s.getData(), s.getLength()); buffer+= s.getLength();
            memcpy(buffer, ":", 1); buffer++; vs = size - s.getLength() - 1;
            if (!v.write(buffer, vs)) return false;
            memcpy(buffer + vs, "\r\n", 2);
            return true;
        }

        template <typename T>
        void setValue(T && t) {
            if constexpr(Concepts::is_stdinitlist_v<std::decay_t<T>>)
            {   // Check if the value type accepts an initializer list or not
                if constexpr( requires { v.setValue(std::forward<T>(t)); }) {
                    v.setValue(std::forward<T>(t));
                } else { // Else only use the first element in the array
                    auto e = t.begin();
                    v.setValue(*e);
                }
            }
            else v.setValue(std::forward<T>(t));
        }

        bool isSet() const {
            std::size_t vs = 0;
            if (!v.write(0, vs)) return false;
            if (vs == 0) return false; // Skip header without value
            return true;
        }
#if MinimizeStackSize == 1
        bool send(BaseSocket & socket)
        {
            if (!v.hasValue()) return true; // Skip headers without any value, since it's wrong anyway
            const ROString & s = Refl::toString(header);

            if (socket.send(s.getData(), s.getLength()) != s.getLength()) return false;
            if (socket.send(":", 1) != 1) return false;
            if (!v.send(socket)) return false;
            return socket.send("\r\n", 2) == 2;
        }
#else
        bool write(Container::TrackedBuffer & buffer)
        {
            std::size_t vs = 0;
            if (!v.write(0, vs)) return false;
            if (vs == 0) return true; // Skip headers without any value, since it's wrong anyway
            const ROString & s = Refl::toString(header);
            if (!buffer.canFit(vs + 3 + s.getLength())) return false;

            if (!buffer.save(s.getData(), s.getLength())) return false;
            if (!buffer.save(":", 1)) return false;
            if (!v.write(&buffer.buffer[buffer.used], vs)) return false;
            buffer.used += vs;
            return buffer.save("\r\n", 2);
        }
#endif
    };
}

#endif
