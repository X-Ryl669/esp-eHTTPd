#ifndef hpp_CPP_HTTPServer_CPP_hpp
#define hpp_CPP_HTTPServer_CPP_hpp


// We need HTTP parsers here
#include "Parser.hpp"
// We need the socket code too for clients and server
#include "Network/Socket.hpp"
// We need intToStr
#include "Strings/RWString.hpp"
// We need error code too
#include "Protocol/HTTP/Codes.hpp"
// We need compile time vectors here to cast some magical spells on types
#include "Container/CTVector.hpp"
#include "Container/RingBuffer.hpp"
// We need streams too
#include "Streams/Streams.hpp"

#include <type_traits>
// We need offsetof for making the container_of macro
#include <cstddef>

#ifndef ClientBufferSize
  #define ClientBufferSize 512
#endif



#define container_of(pointer, type, member)                                                        \
  (reinterpret_cast<type*>((reinterpret_cast<char*>(pointer) - offsetof(type, member))))


namespace Network::Servers::HTTP
{
#ifdef UseTLSServer
    typedef MBTLSSocket Socket;
#else
    typedef BaseSocket Socket;
#endif

    static constexpr const char HTTPAnswer[] = "HTTP/1.1 ";
    static constexpr const char EOM[] = "\r\n\r\n";
    static constexpr const char BadRequestAnswer[] = "HTTP/1.1 400 Bad request\r\n\r\n";
    static constexpr const char EntityTooLargeAnswer[] = "HTTP/1.1 413 Entity too large\r\n\r\n";
    static constexpr const char InternalServerErrorAnswer[] = "HTTP/1.1 500 Internal server error\r\n\r\n";
    static constexpr const char NotFoundAnswer[] = "HTTP/1.1 404 Not found\r\n\r\n";
    static constexpr const char ChunkedEncoding[] = "Transfer-Encoding:chunked\r\n";
    static constexpr const char ConnectionClose[] = "Connection:close\r\n";

    using namespace Protocol::HTTP;

    /** The current client parsing state */
    enum class ClientState
    {
        Error       = 0,
        Processing  = 1,
        NeedRefill  = 2,
        Done        = 3,
    };


    namespace Details
    {
        // This works when they are all the same type
        template<typename Result, typename... Ts>
        Result& runtime_get(std::size_t i, std::tuple<Ts...>& t)  {
            using Tuple = std::tuple<Ts...>;

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

    struct Unobtainium {};
    template <class... T> struct always_false : std::false_type {};
    template <> struct always_false<Unobtainium> : std::true_type {};

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


        bool sendHeaders(Container::TrackedBuffer & buffer)
        {
            return [&]<std::size_t... Is>(std::index_sequence<Is...>)  {
                return (std::get<Is>(headers).write(buffer) && ...);
            }(std::make_index_sequence<sizeof...(Header)>{});
        }
    };

    /** Convert the list of headers you're expecting to the matching HeadersArray the library is using */
    template <Headers ... allowedHeaders>
    struct ToHeaderArray {
        static constexpr auto headersArray = Container::getUnique<std::array{allowedHeaders...}, std::array{Headers::Authorization, Headers::Connection}>();
        typedef HeadersArray<headersArray, decltype(Container::makeTypes<Details::MakeRequest, headersArray>())> Type;
    };

    /** Convert the list of headers you're expecting to the matching HeadersArray the library is using */
    template <Headers ... allowedHeaders>
    struct ToPostHeaderArray {
        static constexpr auto headersArray = Container::getUnique<std::array{allowedHeaders...}, std::array{Headers::ContentType, Headers::ContentLength, Headers::Connection}>();
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
        static constexpr auto headersArray = Container::getUnique<std::array<Headers, HeaderCount>{answerHeaders...}, std::array<Headers, 1> {Headers::WWWAuthenticate}>();
        typedef AnswerHeadersArray<headersArray, decltype(Container::makeTypes<Details::MakeAnswer, headersArray>())> Type;
    };


    /** Store the result of a form that's was posted  */
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

    /** Store the result of a form that's was posted  */
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


    /** A client which is linked with a single session.
        There's a fixed possible number of clients while a server is started to avoid dynamic allocation (and memory fragmentation)
        Thus a client is identified by its index in the client array */
    struct Client
    {
        /** The client socket */
        Socket      socket;
        /** The current parsing status for this client.
            The state machine is like this:
            @code
            [ Invalid ] ==> Request line incomplete => [ ReqLine ]
            [ ReqLine ] ==> Request line complete => [ RecvHeaders ]
            [ RecvHeader ] ==> \r\n\r\n found ? => [ HeadersDone ] (else [NeedRefillHeaders], currently not implemented)
            [ HeadersDone ] ==> Content received? => [ ReqDone ]
            @endcode */
        enum ParsingStatus
        {
            Invalid = 0,
            ReqLine,
            RecvHeaders,
            NeedRefillHeaders, // Currently not implemented, used to trigger route's processing for emptying the recv buffer in case the request doesn't fill the available buffer
            HeadersDone,
            ReqDone,

        } parsingStatus;

        /** The buffer where all the per-request data is saved */
        Container::TranscientVault<ClientBufferSize> recvBuffer;
        /** The current request as received and parsed by the server */
        RequestLine reqLine;

        /** The content length for the answer */
        std::size_t answerLength;
        Code        replyCode;

        /** Send the client answer as expected */
        template <typename T>
        bool sendAnswer(T && clientAnswer, bool close = false) {
            if (!sendStatus(clientAnswer.getCode())) return false;

            // We'll be loosing the URI content when we clear the recvBuffer for sending data back, so store the
            // request URI on the stack for logging purpose below
            char * URI = (char*)alloca(reqLine.URI.absolutePath.getLength());
            memcpy(URI, reqLine.URI.absolutePath.getData(), reqLine.URI.absolutePath.getLength());

            recvBuffer.reset();
            // Force closing the connection, we don't support Keep-Alive connections by default TODO
            if (!clientAnswer.template hasValidHeader<Headers::Connection>())
                socket.send(ConnectionClose, sizeof(ConnectionClose) - 1);

            if (!clientAnswer.sendHeaders(*this)) return false;
            auto && stream = clientAnswer.getInputStream(socket);
            std::size_t answerLength = 0;
            if constexpr (!std::is_same_v<std::decay_t<decltype(stream)>, std::nullptr_t>)
            {
                answerLength = stream.getSize();
                if (answerLength)
                {
                    if (!sendSize(answerLength))
                    {
                        SLog(Level::Info, "Client %s [%.*s](%u): 523%s", socket.address, reqLine.URI.absolutePath.getLength(), URI, answerLength, close ? " closed" : "");
                        return false;
                    }

                    // Send the content now
                    while (reqLine.method != Method::HEAD)
                    {
                        std::size_t p = stream.read(recvBuffer.getTail(), recvBuffer.freeSize());
                        if (!p) break;

                        socket.send((const char*)recvBuffer.getTail(), p);
                    }

                } else if (stream.hasContent() && reqLine.method != Method::HEAD)
                {
                    if (!clientAnswer.template hasValidHeader<Headers::TransferEncoding>()) {
                        // Need to send a transfer encoding header if we don't have a size for the content and it's not done by the client's answer by itself
                        socket.send(ChunkedEncoding, sizeof(ChunkedEncoding) - 1);
                    }

                    if (!clientAnswer.sendContent(*this, answerLength))
                    {
                        SLog(Level::Info, "Client %s [%.*s](%u): 524%s", socket.address, reqLine.URI.absolutePath.getLength(), URI, 0, close ? " closed" : "");
                        return false;
                    }
                } else if (!stream.hasContent())
                {
                    if (!sendSize(0))
                    {
                        SLog(Level::Info, "Client %s [%.*s](%u): 525%s", socket.address, reqLine.URI.absolutePath.getLength(), URI, answerLength, close ? " closed" : "");
                        return false;
                    }
                }
            } else
            {
                if (!sendSize(0))
                {
                    SLog(Level::Info, "Client %s [%.*s](%u): 525%s", socket.address, reqLine.URI.absolutePath.getLength(), URI, answerLength, close ? " closed" : "");
                    return false;
                }
            }

            SLog(Level::Info, "Client %s [%.*s](%u): %d%s", socket.address, reqLine.URI.absolutePath.getLength(), URI, answerLength, (int)clientAnswer.getCode(), close ? " closed" : "");
            parsingStatus = ReqDone;
            if (close) reset();
            return true;
        }

        bool sendStatus(Code replyCode)
        {
            char buffer[5] = { };
            intToStr((int)replyCode, buffer, 10);
            buffer[3] = ' ';

            socket.send(HTTPAnswer, strlen(HTTPAnswer));
            socket.send(buffer, strlen(buffer));
            socket.send(Refl::toString(replyCode), strlen(Refl::toString(replyCode)));
            socket.send(EOM, 2);
            return true;
        }
        bool sendSize(std::size_t length)
        {
            static const char hdr[] = { ':' };
            char buffer[sizeof("18446744073709551615")] = { };
            socket.send(Refl::toString(Headers::ContentLength), strlen(Refl::toString(Headers::ContentLength)));
            socket.send(hdr, 1);
            intToStr((int)length, buffer, 10);
            socket.send(buffer, strlen(buffer));
            socket.send(EOM, strlen(EOM));
            return true;
        }
        bool reply(Code statusCode, const ROString & msg, bool close = false);
        bool reply(Code statusCode);

        bool closeWithError(Code code) { return reply(code); }


        uint32 persistVaultSize = 0;
        inline bool hasPersistedHeaders() const { return recvBuffer.vaultSize() > persistVaultSize; }

        template <typename Headers>
        Client & routeFound(Headers & headers)
        {
            // Need to reload all headers from the vault here (if any saved)
            if (hasPersistedHeaders())
            {   // Reload the header from the vault here
                headers.loadFromVault(recvBuffer);
                // Discard the headers from the vault so we can have space for any new string to persist
                recvBuffer.resetVault(persistVaultSize);
            }
            return *this;
        }

        template <typename Headers>
        ClientState saveHeaders(Headers & headers)
        {
            if (parsingStatus == NeedRefillHeaders)
            {
                // Save the actual used size for persisted strings (that won't be reset on the next parsing)
                persistVaultSize = recvBuffer.vaultSize();
                // Save the current header array to the vault
                if (!headers.saveInVault(recvBuffer)) {
                    closeWithError(Code::InternalServerError);
                    return ClientState::Error;
                }
            }
            return ClientState::NeedRefill;
        }

        template <typename T>
        bool fetchContent(const auto & headers, T & content)
        {
            // This must be called after the header are parsed
            if (parsingStatus != HeadersDone) return false;

            // Extract the expected content length from the current receiving buffer
            auto length = headers.template getHeader<Headers::ContentLength>();
            size_t expLength = length.getValueElement(0);

            auto type = headers.template getHeader<Headers::ContentType>();
            switch(type.getValueElement(0))
            {
                case MIMEType::multipart_formData:
                {   // TODO: Support multipart encoding
                    return false;
                }
                case MIMEType::application_xWwwFormUrlencoded:
                    if constexpr(requires{ typename T::IsAFormPost; })
                    {
                        if (recvBuffer.maxSize() < expLength)
                            // Request is too big for us to parse, so let's bail out
                            return false;

                        if (recvBuffer.getSize() < expLength)
                        {
                            // Need to fetch the missing content
                            Error ret = socket.recv((char*)recvBuffer.getTail(), expLength - recvBuffer.getSize());
                            if (ret.isError()) return false;
                            recvBuffer.stored(ret.getCount());
                        }

                        ROString input = recvBuffer.getView<ROString>();
                        content.parse(input);
                        return true;
                    } else return false; // You need to use a FormPost class here to get the posted form
                default:
                    if constexpr(requires{ content.write((char*)0, 0); })
                    {
                        // Save what we've already received
                        std::size_t len = content.write(recvBuffer.getHead(), recvBuffer.getSize());
                        if (len != recvBuffer.getSize()) return false;
                        recvBuffer.resetTranscient(0);

                        Streams::Socket in(socket);
                        expLength -= len;
                        len = Streams::copy(in, content, recvBuffer.getTail(), recvBuffer.freeSize(), expLength);
                        return len == expLength;
                    }
                    else
                        return false;
            }
        }

        bool parse() {
            ROString buffer = recvBuffer.getView<ROString>();
            switch (parsingStatus)
            {
            case Invalid:
            {   // Check if we had a complete request line
                parsingStatus = ReqLine;
            } // Intentionally no break here
            case ReqLine:
            {
                if (buffer.Find("\r\n") != buffer.getLength())
                {
                    // Potential request line found, let's parse it to check if it's full
                    if (ParsingError err = reqLine.parse(buffer); err != MoreData)
                        return closeWithError(Code::BadRequest);
                    // Got the request line, let's move to the headers now
                    // A classical HTTP server will continue parsing headers here
                    // But we aren't a classical HTTP server, there's no point in parsing header if there's no route to match for it
                    // So we'll stop here and the server will match the routes that'll take over the parsing from here.
                    parsingStatus = RecvHeaders;
                    if (!reqLine.URI.normalizePath()) return closeWithError(Code::BadRequest);
                    // Make sure we save the current normalized URI since it's used by the routes
                    if (!reqLine.persist(recvBuffer, (std::size_t)((const uint8*)buffer.getData() - recvBuffer.getHead()))) return closeWithError(Code::InternalServerError);

                    // We don't need the request line anymore, let's drop it from the receive buffer
                    persistVaultSize = recvBuffer.vaultSize();
                    buffer = recvBuffer.getView<ROString>();
                } else {
                    // Check if we can ultimately receive a valid request?
                    return recvBuffer.freeSize() ? true : closeWithError(Code::EntityTooLarge);
                }
            }
            // Intentionally no break here
            case RecvHeaders:
            case NeedRefillHeaders:
                if (buffer.Find(EOM) != buffer.getLength() || buffer == "\r\n") // No header here is valid too
                {
                    parsingStatus = HeadersDone;
                    return true;
                 } else {
                    if (recvBuffer.freeSize()) return true;

                    // Here, we cut the already parsed headers where we've extracted information from.
                    if (recvBuffer.getSize() < 64)
                    {   // There is not enough space left for the transcient buffer (the vault is already too large) to actually make any process, so let's just reject this as an error
                        return closeWithError(Code::EntityTooLarge);
                    }
                    // Just remember to refill headers in the route when we know more about it
                    parsingStatus = NeedRefillHeaders;
                    return true;
                }
            case HeadersDone:
            case ReqDone: break;
            }
            return true;
        }
        /** Get the requested, normalized URI, helper function */
        ROString getRequestedPath() const { return reqLine.URI.onlyPath(); }
        /** Check if the client is valid */
        bool isValid() const { return socket.isValid(); }


    protected:
        /** Reset this client state and buffer. This is called from the server's accept method before actually using the client */
        void reset() {
            recvBuffer.reset();
            reqLine.reset();
            parsingStatus = Invalid;
            socket.reset();
            answerLength = 0;
            persistVaultSize = 0;
        }

    };


    /** A client answer structure.
        This is a convenient, template type, made to build an answer for an HTTP request.
        There are 3 different possible answer type supported by this library:
        1. A DIY answer, where you take over the client's socket, and set whatever you want (likely breaking HTTP protocol) - Not recommended
        2. A simple answer, with basic headers (like Content-Type) and a fixed string as output.
        3. A more complex answer with basic headers and you want to have control over the output stream (in that case, you'll give an input stream to the library to consume)

        It's templated over the list of headers you intend to use in the answer (it's a statically built for saving both binary space, memory and logic).
        In all cases, Content-Length is commputed by the library. Transfer-Encoding can be set up by you or the library depending on the stream itself
        @param getContentFunc   A content function that returns a stream that can be used by the client to send the answer. If not provided, defaults to a simple string
        @param answerHeaders    A list of headers you are expecting to answer */
    template< typename Child, Headers ... answerHeaders>
    struct ClientAnswer
    {
        // Construct the answer header array
        typedef ToAnswerHeader<answerHeaders...>::Type ExpectedHeaderArray;
        ExpectedHeaderArray headers;
        /** The reply status code to use */
        Code                replyCode;
        Child * c() { return static_cast<Child*>(this); }

        auto getInputStream(Socket & socket) {
            if constexpr (requires { typename Child::getInputStream ; })
                return c()->getInputStream(socket);
            else return nullptr;
        }
        void setCode(Code code) { this->replyCode = code; }
        Code getCode() const { return this->replyCode; }

        template <Headers h, typename Value>
        void setHeaderIfUnset(Value && v) { headers.template setHeaderIfUnset<h>(std::forward<Value>(v)); }
        template <Headers h, typename Value>
        void setHeader(Value && v) { headers.template getHeader<h>().setValue(std::forward<Value>(v)); }
        template <Headers h>
        bool hasValidHeader() const { return headers.template hasValidHeader<h>(); }

        bool sendHeaders(Client & client)
        {
            Container::TrackedBuffer buffer { client.recvBuffer.getTail(), client.recvBuffer.freeSize() };
            if (!headers.sendHeaders(buffer)) return false;
            return client.socket.send(buffer.buffer, buffer.used) == buffer.used;
        }

        bool sendContent(Client & client, std::size_t & totalSize) {

            if constexpr (&Child::sendContent != &ClientAnswer::sendContent)
                return c()->sendContent(client, totalSize);
            else return true;
        }
        ClientAnswer(Code code = Code::Invalid) : replyCode(code) {}
    };


    /** A simple answer with the given MIME type and message */
    template <MIMEType m = MIMEType::text_plain>
    struct SimpleAnswer : public ClientAnswer<SimpleAnswer<m>, Headers::ContentType>
    {
        Streams::MemoryView getInputStream(Socket&) { return Streams::MemoryView(msg); }
        const ROString & msg;
        SimpleAnswer(Code code, const ROString & msg) : SimpleAnswer::ClientAnswer(code), msg(msg)
        {
            this->template setHeader<Headers::ContentType>(m);
        }
    };
    /** The shortest answer: only a status code */
    struct CodeAnswer : public ClientAnswer<CodeAnswer>
    {
        CodeAnswer(Code code) : CodeAnswer::ClientAnswer(code) {}
    };


    /** A JSON escaping dynamic function wrapper. This is used to escape strings so they can respect the JSON format */
    static size_t computeJSONStringRequiredSize(const ROString & input)
    {
        // First pass, count the number of required output char
        size_t count = 0; const char * p = input.getData();
        for (size_t i = 0; i < input.getLength(); i++) {
            switch (p[i]) {
            case '"': count += 2; break;
            case '\\': count += 2; break;
            case '\b': count += 2; break;
            case '\f': count += 2; break;
            case '\n': count += 2; break;
            case '\r': count += 2; break;
            case '\t': count += 2; break;
            default:
                if ((uint8)p[i] <= '\x1f') {
                    count += 6;
                } else {
                    count++;
                }
            }
        }
        return count;
    }

    static RWString escapeJSONString(const ROString & input)
    {
        size_t count = computeJSONStringRequiredSize(input);
        RWString ret(0, count);
        char * o = (char*)ret.getData();
        static char hexDigits[] = "0123456789abcdef";
        const char * p = input.getData();
        for (size_t i = 0; i < input.getLength(); i++) {
            switch (p[i]) {
            case '"': *o++ = '\\'; *o++ = '"';   break;
            case '\\': *o++ = '\\'; *o++ = '\\'; break;
            case '\b': *o++ = '\\'; *o++ = 'b'; break;
            case '\f': *o++ = '\\'; *o++ = 'f'; break;
            case '\n': *o++ = '\\'; *o++ = 'n'; break;
            case '\r': *o++ = '\\'; *o++ = 'r'; break;
            case '\t': *o++ = '\\'; *o++ = 't'; break;
            default:
                if ((uint8)p[i] <= '\x1f') {
                    *o++ = '\\'; *o++ = 'u'; *o++ = '0'; *o++ = '0';
                    *o++ = hexDigits[p[i] >> 4]; *o++ = hexDigits[p[i] & 0xF];
                } else {
                    *o++ = p[i];
                }
            }
        }
        *o = 0;
        return ret;
    }


    /** The get a chunk function that should follow this signature:
        @code
            ROString callback()
        @endcode

        Return an empty string to stop being called back
        */
    template <typename Func>
    concept GetChunkCallback = requires (Func f) {
        // Make sure the signature matches
        ROString {f()};
    };

    /** Capture the client's socket when it's time for answering and call the given callback you'll fill with data */
    template <Headers ... answerHeaders>
    struct HeaderSet : public ClientAnswer<HeaderSet<answerHeaders...>, answerHeaders...>
    {
        HeaderSet(Code code) : HeaderSet::ClientAnswer(code) {}
        HeaderSet() {}

        template <typename ... Values>
        HeaderSet(Values &&... values) {
            // Set all the values for each header
            [&]<std::size_t... Is>(std::index_sequence<Is...>)  {
                return ((this->template setHeader<answerHeaders>(std::forward<Values>(values)), true) && ...);
            }(std::make_index_sequence<sizeof...(answerHeaders)>{});
        }

        template <typename ... Values>
        HeaderSet(std::initializer_list<Values> && ... values) {
            // Set all the values for each header
            [&]<std::size_t... Is>(std::index_sequence<Is...>)  {
                return ((this->template setHeader<answerHeaders>(std::forward<std::initializer_list<Values>>(values)), true) && ...);
            }(std::make_index_sequence<sizeof...(answerHeaders)>{});
        }
    };

    /** Here we are capturing a lambda function, so we don't know the type and we don't want to perform type erasure for size reasons
        This structure will use chunked transfer to send pieces of the answer */
    template <typename T, typename HS>
    struct CaptureAnswer
    {
        Streams::Empty getInputStream(Socket&) { return Streams::Empty{}; }
        /** This constructor is used for deduction guide */
        template <typename V>
        CaptureAnswer(Code code, V && v, T f) : headers(std::move(v)), callbackFunc(f)
        {
            headers.setCode(code);
        }

        // Proxy the ClientAnswer interface here, using headers' member
        bool sendContent(Client & client, std::size_t & totalSize) {
            Streams::ChunkedOutput o{client.socket};
            totalSize = 0;
            ROString s = std::move(callbackFunc());
            while (s)
            {
                if (o.write(s.getData(), s.getLength()) != s.getLength()) return false;
                totalSize += (std::size_t)s.getLength();
                s = std::move(callbackFunc());
            }
            // Need to finish sending the flux
            o.write(nullptr, 0);
            return true;
        }
        template <Headers h, typename Value>
        void setHeaderIfUnset(Value && v) { headers.template setHeaderIfUnset<h>(std::forward<Value>(v)); }
        template <Headers h, typename Value>
        void setHeader(Value && v) { headers.template setHeader<h>(std::forward<Value>(v)); }
        template <Headers h>
        bool hasValidHeader() const { return headers.template hasValidHeader<h>(); }
        Code getCode() const { return headers.getCode(); }
        bool sendHeaders(Client & client) { return headers.sendHeaders(client); }
        operator HS & () { return headers; }

        /** The lambda function we've captured */
        T callbackFunc;
        /** Aggregate header type */
        HS headers;
    };
    /** Add a deducing guide for the lambda function */
    template<typename T, typename V>
    CaptureAnswer(Code, V, T) -> CaptureAnswer<std::decay_t<T>, V>;

    /** A answer solution that's returning the content of the given file */
    template <typename InputStream, Headers ... answerHeaders>
    struct FileAnswer : public ClientAnswer<FileAnswer<InputStream, answerHeaders...>, Headers::ContentType, answerHeaders...>
    {
        /** Get the expected MIME type from the given file extension */
        constexpr static MIMEType getMIMEFromExtension(const ROString ext) {
            using namespace CompileTime;
            MIMEType mimeType = MIMEType::application_octetStream;
            switch(constHash(ext.getData(), ext.getLength()))
            {
            case "html"_hash: case "htm"_hash: mimeType = MIMEType::text_html; break;
            case "css"_hash:                   mimeType = MIMEType::text_css; break;
            case "js"_hash:                    mimeType = MIMEType::application_javascript; break;
            case "png"_hash:                   mimeType = MIMEType::image_png; break;
            case "jpg"_hash: case "jpeg"_hash: mimeType = MIMEType::image_jpeg; break;
            case "gif"_hash:                   mimeType = MIMEType::image_gif; break;
            case "svg"_hash:                   mimeType = MIMEType::image_svg__xml; break;
            case "webp"_hash:                  mimeType = MIMEType::image_webp; break;
            case "xml"_hash:                   mimeType = MIMEType::application_xml; break;
            case "txt"_hash:                   mimeType = MIMEType::text_plain; break;
            default: break;
            }
            return mimeType;
        }

        InputStream & getInputStream(Socket &) { return stream; }

        FileAnswer(const char* path) : FileAnswer::ClientAnswer(Code::NotFound), stream(path)
        {
            // Check if the file is found
            if (stream.hasContent())
            {
                this->setCode(Code::Ok);
                // Then find out the extension for the file in order to deduce the MIME type to use
                this->template setHeader<Headers::ContentType>(getMIMEFromExtension(ROString(path).fromLast(".")));
            } else this->template setHeader<Headers::ContentType>(MIMEType::Invalid);
        }

        FileAnswer(const ROString & path, const ROString & content) : FileAnswer::ClientAnswer(Code::NotFound), stream(content)
        {
            // Check if the file is found
            if (stream.hasContent())
            {
                this->setCode(Code::Ok);
                // Then find out the extension for the file in order to deduce the MIME type to use
                this->template setHeader<Headers::ContentType>(getMIMEFromExtension(path.fromLast(".")));
            } else this->template setHeader<Headers::ContentType>(MIMEType::Invalid);
        }        
        InputStream stream;
    };

    bool Client::reply(Code statusCode, const ROString & msg, bool close)
    {
        // Check if the msg is in the recv buffer (can happen with request with content), and in that case, it need to be persisted in the vault
        // or it'll be overwritten while replying
        if (recvBuffer.contains(msg.getData()))
        {   // Persist it
            if (!Container::persistString(const_cast<ROString&>(msg), recvBuffer, recvBuffer.getSize())) return false;
        }
        return sendAnswer(SimpleAnswer<MIMEType::text_plain>{statusCode, msg }, close);
    }
    bool Client::reply(Code statusCode) { return sendAnswer(CodeAnswer{statusCode}, true); }
}


#endif
