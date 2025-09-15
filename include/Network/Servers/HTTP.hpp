#ifndef hpp_Server_HTTP_hpp
#define hpp_Server_HTTP_hpp


// We need headers array too
#include "Network/Common/HTTPMessage.hpp"
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
// We need forms too
#include "Forms.hpp"

#include <type_traits>


#ifndef ClientBufferSize
  #define ClientBufferSize 1024
#endif

namespace Network::Servers::HTTP
{
    using namespace Protocol::HTTP;
    using namespace Network::Common::HTTP;

#if UseTLSServer == 1
    typedef MBTLSSocket Socket;
#else
    typedef BaseSocket Socket;
#endif

    static constexpr const char HTTPAnswer[] = "HTTP/1.1 ";
    static constexpr const char BadRequestAnswer[] = "HTTP/1.1 400 Bad request\r\n\r\n";
    static constexpr const char EntityTooLargeAnswer[] = "HTTP/1.1 413 Entity too large\r\n\r\n";
    static constexpr const char InternalServerErrorAnswer[] = "HTTP/1.1 500 Internal server error\r\n\r\n";
    static constexpr const char NotFoundAnswer[] = "HTTP/1.1 404 Not found\r\n\r\n";
    static constexpr const char ChunkedEncoding[] = "Transfer-Encoding:chunked\r\n\r\n";
    static constexpr const char ConnectionClose[] = "Connection:close\r\n";

    /** The current client parsing state */
    enum class ClientState
    {
        Error       = 0,
        Processing  = 1,
        NeedRefill  = 2,
        Done        = 3,
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
        /** Whether to close or keep the connection open after this request.
            When a connection is kept open, each loop without activity will decrease the TTL until it reaches 0 and force the client connection to close */
        uint8       timeToLive = 0;

        /** The content length for the answer */
        std::size_t answerLength;
        Code        replyCode;

        /** Send the client answer as expected */
        template <typename T>
        bool sendAnswer(T && clientAnswer) {
            if (!sendStatus(clientAnswer.getCode())) return false;

            // We'll be loosing the URI content when we clear the recvBuffer for sending data back, so store the
            // request URI on the stack for logging purpose below
            char * URI = (char*)alloca(reqLine.URI.absolutePath.getLength());
            memcpy(URI, reqLine.URI.absolutePath.getData(), reqLine.URI.absolutePath.getLength());

            recvBuffer.reset();
            // Force closing the connection if required or asked, we don't send the Connection:keep-alive header since it's the default in HTTP/1.1
            if (!timeToLive)
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
                        SLog(Level::Info, "Client %s [%.*s](%u): %d%s", socket.address, (int)reqLine.URI.absolutePath.getLength(), URI, answerLength, 523, !timeToLive ? " closed" : "");
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
                        SLog(Level::Info, "Client %s [%.*s](%u): %d%s", socket.address, (int)reqLine.URI.absolutePath.getLength(), URI, 0U, 524, !timeToLive ? " closed" : "");
                        return false;
                    }
                } else if (!stream.hasContent())
                {
                    if (!sendSize(0))
                    {
                        SLog(Level::Info, "Client %s [%.*s](%u): %d%s", socket.address, (int)reqLine.URI.absolutePath.getLength(), URI, answerLength, 525, !timeToLive ? " closed" : "");
                        return false;
                    }
                }
            } else
            {
                if (!sendSize(0))
                {
                    SLog(Level::Info, "Client %s [%.*s](%u): %d%s", socket.address, (int)reqLine.URI.absolutePath.getLength(), URI, answerLength, 525, !timeToLive ? " closed" : "");
                    return false;
                }
            }

            SLog(Level::Info, "Client %s [%.*s](%u): %d%s", socket.address, (int)reqLine.URI.absolutePath.getLength(), URI, answerLength, (int)clientAnswer.getCode(), !timeToLive ? " closed" : "");
            parsingStatus = ReqDone;
            reset();
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
        bool sendSize(std::size_t length) { return Common::HTTP::sendSize(socket, length); }
        bool reply(Code statusCode, const ROString & msg, bool close = false);
        bool reply(Code statusCode);

        bool closeWithError(Code code) { forceCloseConnection(); return reply(code); }
        void forceCloseConnection() { timeToLive = 0; }


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
            timeToLive = 255;
            ROString buffer = recvBuffer.getView<ROString>();
            switch (parsingStatus)
            {
            case Invalid:
            {   // Check if we had a complete request line
                parsingStatus = ReqLine;
            }
            [[fallthrough]];
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
            [[fallthrough]];
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
        /** Decrease time to live (and close the socket if required)
            @return true if closed */
        bool tickTimeToLive() {
            if (!timeToLive) return false;
            if (--timeToLive == 0)
            {
                reset();
                return true;
            }
            return false;
        }
        /** Socket was accepted */
        void accepted() { timeToLive = 255; }
        /** Socket was remotely closed */
        void closed() { timeToLive = 0; reset(); }

    protected:
        /** Reset this client state and buffer. This is called from the server's accept method before actually using the client */
        void reset() {
            recvBuffer.reset();
            reqLine.reset();
            parsingStatus = Invalid;
            if (!timeToLive) socket.reset();
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
    struct ClientAnswer : public CommonHeader<ClientAnswer<Child, answerHeaders...>, Socket, answerHeaders...>
    {
        Child * c() { return static_cast<Child*>(this); }

        auto getInputStream(Socket & socket) {
            if constexpr (requires { typename Child::getInputStream ; })
                return c()->getInputStream(socket);
            else return nullptr;
        }

        bool sendHeaders(Client & client)
        {
#if MinimizeStackSize == 1
            return ClientAnswer::CommonHeader::sendHeaders(client.socket);
#else
            Container::TrackedBuffer buffer { client.recvBuffer.getTail(), client.recvBuffer.freeSize() };
            return ClientAnswer::CommonHeader::sendHeaders(client.socket, buffer);
#endif
        }

        bool sendContent(Client & client, std::size_t & totalSize) {

            if constexpr (&Child::sendContent != &ClientAnswer::sendContent)
                return c()->sendContent(client, totalSize);
            else return true;
        }
        ClientAnswer(Code code = Code::Invalid) : ClientAnswer::CommonHeader(code) {}
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
            ROString s = callbackFunc();
            while (s)
            {
                if (o.write(s.getData(), s.getLength()) != s.getLength()) return false;
                totalSize += (std::size_t)s.getLength();
                s = callbackFunc();
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

        /** Aggregate header type */
        HS headers;
        /** The lambda function we've captured */
        T callbackFunc;
    };
    /** Add a deducing guide for the lambda function */
    template<typename T, typename V>
    CaptureAnswer(Code, V, T) -> CaptureAnswer<std::decay_t<T>, V>;

    /** A answer solution that's returning the content of the given file */
    template <typename InputStream, Headers ... answerHeaders>
    struct FileAnswer : public ClientAnswer<FileAnswer<InputStream, answerHeaders...>, Headers::ContentType, answerHeaders...>
    {
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
        if (close) timeToLive = 0;
        return sendAnswer(SimpleAnswer<MIMEType::text_plain>{statusCode, msg });
    }
    bool Client::reply(Code statusCode) { return sendAnswer(CodeAnswer{statusCode}); }
}


#endif
