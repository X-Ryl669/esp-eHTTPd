#ifndef hpp_Client_HTTP_hpp
#define hpp_Client_HTTP_hpp


// We need headers array too
#include "Network/Common/HeadersArray.hpp"
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


#include <type_traits>

#if BuildClient == 1


#ifndef ClientBufferSize
  #define ClientBufferSize 1024
#endif

namespace Network::Clients::HTTP
{
    using namespace Protocol::HTTP;
    using namespace Network::Common::HTTP;

    template <typename Child, Headers ... headersInterestedIn>
    struct EventCallback
    {
        static constexpr std::array<Headers, sizeof...(headersInterestedIn)> HeadersList = { headersInterestedIn... };
        /** A useful check-me method to quickly abort parsing if we aren't interested in that header */
        bool isInterestedIn(Headers h) const { return ((h == headersInterestedIn) || ...); }
        /** A useful check-me method to quickly abort parsing if we aren't interested in that header */
        bool isInterestedIn(ROString header) const { return ((header == Refl::toString(headersInterestedIn)) || ...); }
        /** To avoid having an large number of virtual method, the following method generic.
            You can build a RequestHeader<h> and call createFrom(rawValue) to get a usable value here like this:
            @code
                switch(h)
                {
                case Headers::ContentLength: {
                    auto hdr = RequestHeader<Headers::ContentLength>::createFrom(value);
                    printf("Content length: %u", hdr.getValueElement(0));
                    break;
                    }
                [...]
                }
            @endcode */
        void headerReceived(Headers h, ROString value) { if constexpr(requires{ Child::headerReceived; }) static_cast<Child*>(this)->headerReceived(h, value); }
        /** Called with the received code for the server answer */
        void serverAnswered(Code code) { if constexpr(requires{ Child::serverAnswered; }) static_cast<Child*>(this)->serverAnswered(code); }
        /** Called when there is data for this answer.
            You don't need to decode the input data (transfer encoding or inflating / decompressing), it's already done for you in the given stream.
            @sa BaseEventCallback for a simply implementation of this */
        template <typename InStream>
        bool dataReceived(InStream & stream, std::size_t totalLength = 0) { if constexpr(requires{ Child::dataReceived; }) return static_cast<Child*>(this)->dataReceived(stream, totalLength); else return true; }
    };


    template <typename OutStream>
    struct BasicEventCallback : public EventCallback< BasicEventCallback<OutStream> >
    {
        OutStream & outStream;

        template <typename InStream>
        bool dataReceived(InStream & stream, std::size_t totalLength = (std::size_t)-1) { return (int64)Streams::copy(stream, outStream, totalLength) >= (int64)totalLength; }
        BasicEventCallback(OutStream & stream) : outStream(stream) {}
    };

    template <typename Callback>
    struct Request
    {
        Method method;
        ROString url;
        ROString additionalHeaders;
        Callback callback;

        Request(auto & callbackArg, Method method, ROString url, ROString additionalHeaders = "") : callback(callbackArg), method(method), url(url), additionalHeaders(additionalHeaders) {}
    };
    // Deduction guide to avoid specifying the output stream type
    template <typename OutStream> Request(OutStream &, Method, ROString, ROString = "") -> Request<OutStream>;

    template <typename InStream, typename Request>
    struct RequestWithTypedStream : public Request
    {
        auto getInputStream(BaseSocket & socket) { return inStream; }
        const char* getStreamType(BaseSocket &)  { return Refl::toString(mime); }

        InStream inStream;
        MIMEType mime;

        template<typename StreamArg, typename ... Args>
        RequestWithTypedStream(StreamArg arg, MIMEType mime, Args && ... args) : RequestWithTypedStream::Request(std::forward<Args>(args)...), inStream(arg), mime(mime) {}
    };

    template <typename Stream, typename Request>
    struct RequestWithStream : public Request
    {
        auto getInputStream(BaseSocket & socket) { return inStream; }

        Stream inStream;

        template<typename StreamArg, typename ... Args>
        RequestWithStream(StreamArg arg, Args && ... args) : RequestWithStream::Request(std::forward<Args>(args)...), inStream(arg) {}
    };

    template <typename Base>
    struct RequestWithExpectedServerCert : public Base
    {
        ROString cert;
        template <typename ... Args>
        RequestWithExpectedServerCert(ROString cert, Args && ... args) : Base(std::forward<Args>(args)...), cert(cert) {}
    };

    // Compile-time proxy wrapper used to enhance logging while capture communication
    template <int Level> struct SocketDumper
    {
        BaseSocket & socket;
        SocketDumper(BaseSocket & socket) : socket(socket) {}

        template <typename ... Args> inline auto connect(Args && ... args) { return socket.connect(std::forward<Args>(args)...); }
        template <typename ... Args> inline auto send(Args && ... args)    { return socket.send(std::forward<Args>(args)...); }
        template <typename ... Args> inline auto recv(Args && ... args)    { return socket.recv(std::forward<Args>(args)...); }
    };

    template <> struct SocketDumper<1>
    {
        BaseSocket & socket;
        SocketDumper(BaseSocket & socket) : socket(socket) {}

        template <typename ... Args> inline auto connect(Args && ... args) { auto r = socket.connect(std::forward<Args>(args)...); SLog(Level::Info, "Connect returned: %d", (int)r); return r; }
        inline auto send(const char * b, const uint32 l)                   { auto r = socket.send(b, l); SLog(Level::Info, "Send returned: %d/%u", (int)r, l); return r; }
        inline auto recv(char * b, const uint32 l, const uint32 m = 0)     { auto r = socket.recv(b, l, m); SLog(Level::Info, "Recv returned: %d/%u", (int)r, l); return r; }
    };

    template <> struct SocketDumper<2>
    {
        BaseSocket & socket;
        SocketDumper(BaseSocket & socket) : socket(socket) {}

        template <typename ... Args> auto connect(const char * h, uint16 p, Args && ... args) { auto r = socket.connect(h, p, std::forward<Args>(args)...); SLog(Level::Info, "Connect to %s:%hu returned: %d", h, p, (int)r); return r; }
        auto send(const char * b, const uint32 l)                                             { auto r = socket.send(b, l); SLog(Level::Info, "Send [%.*s] returned: %d/%u", l, b, (int)r, l); return r; }
        auto recv(char * b, const uint32 l, const uint32 m = 0)                               { auto r = socket.recv(b, l, m); SLog(Level::Info, "Recv returned: %d/%u [%.*s]", (int)r, l, r.getCount() > 0 ? r.getCount() : 0, b); return r; }
    };

    /** A native and simple HTTP client library reusing the code of the HTTP server to avoid duplicate in your binary.
     */

    struct Client
    {
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
            HeadersDone,
        };

        template <int verbosity, typename Request>
        Code sendRequest(Request & request)
        {
            RWString currentURL = request.url;
            int redirectCount = 3;

            while (redirectCount)
            {
                Code code = sendRequestImpl<verbosity>(request, currentURL);
                if (code == Code::MovedForever || code == Code::MovedTemporarily || code == Code::TemporaryRedirect)
                {   // Handle redirects
                    redirectCount--;
                    continue;
                }
                else if (code == Code::Unauthorized)
                {   // Handle authentication
                    redirectCount--;
                    continue;
                }
                return code;
            }
            // Too many redirections leads to stopping the bleeding
            return Code::ClientRequestError;
        }

        template <int verbosity, typename Request>
        Code sendRequestImpl(Request & request, RWString & currentURL)
        {
            // Parse the given URL to check for supported features
            ROString url = currentURL;
            ROString scheme = url.splitFrom("://");
            if (scheme != "http" && scheme != "https") return Code::ClientRequestError;
#if UseTLSClient == 0
            if (scheme == "https") return Code::ClientRequestError;
#endif
            ROString credentials = url.splitFrom("@");
            ROString authority = url.splitUpTo("/");
            ROString qdn = authority.upToLast(":");
            uint16 port = scheme == "http" ? 80 : 443;
            if (qdn.getLength() != authority.getLength())
                port = (uint32)(int)authority.fromLast(":");

            if (credentials)
                // Not supported yet
                return Code::ClientRequestError;

            // Parse the request URI scheme to know what kind of socket to build
            BaseSocket * _socket =
#if UseTLSClient != 0
                scheme == "https" ? new MBTLSSocket :
#endif
                new BaseSocket;

            SocketDumper<verbosity> socket(*_socket);

            // Prepare the request line now
            ROString uri = request.url.midString(request.url.getLength() - url.getLength() - (url[0] == '/'), url.getLength() + (url[0] == '/'));
            RWString reqLine = RWString(Refl::toString(request.method)) + " " + (uri ? uri : "/") + " HTTP/1.1\r\n";
            // Prepare the host line too
            RWString hostHeader = RWString("Host:") + qdn + "\r\n";

            // Connect to the server
            char * host = (char*)alloca(qdn.getLength() + 1);
            memcpy(host, qdn.getData(), qdn.getLength());
            host[qdn.getLength()] = 0;
            Error err = Success;
            if constexpr (requires{request.cert;}) {
                err = socket.connect(host, port, 5000, &request.cert);
            } else {
                err = socket.connect(host, port, 5000);
            }
            if (err != Success) {
                SLog(Level::Error, "Connect error: %d", (int)err);
                return Code::ClientRequestError;
            }

            // Send the request now
            if (socket.send(reqLine.getData(), reqLine.getLength()) != reqLine.getLength())
                return Code::Unavailable;
            if (socket.send(hostHeader.getData(), hostHeader.getLength()) != hostHeader.getLength())
                return Code::Unavailable;
            // Send the headers
            if (socket.send(request.additionalHeaders.getData(), request.additionalHeaders.getLength()) != request.additionalHeaders.getLength())
                return Code::Unavailable;
            // TODO: Support GZIP and Deflate
            const char acceptEncoding[] = "Accept-Encoding:identity\r\n";
            if (socket.send(acceptEncoding, sizeof(acceptEncoding) - 1) != sizeof(acceptEncoding) - 1)
                return Code::Unavailable;

            // Check if we have some content to send
            if constexpr (requires{request.getInputStream(*socket);}) {
                auto && stream = request.getInputStream(*socket);
                // If we know the type of file to send, tell the server about it
                if constexpr (requires{request.getStreamType(*socket);}) {
                    hostHeader = "Content-Type:" + request.getStreamType(*socket) + "\r\n";
                    if (socket.send(hostHeader.getData(), hostHeader.getLength()) != hostHeader.getLength())
                        return Code::Unavailable;
                }
                // Send the content length header
                std::size_t contentLength = stream.getSize();
                if (contentLength)
                {
                    if (!sendSize(*socket, contentLength)) return Code::ClientRequestError;

#if MinimizeStackSize == 1
                    uint8 buffer[1024];
#endif
                    // Send the content now
                    while (true)
                    {
                        std::size_t p = stream.read(buffer, ArrSz(buffer));
                        if (!p) break;

                        socket.send((const char*)buffer, p);
                    }
                }
                else // TODO: Support chunked encoding for the request
                    return Code::ClientRequestError;
            } else {
                // Send the end of request and start receiving the answer
                if (socket.send(EOM, 2) != 2) return Code::Unavailable;
            }

            // Receive HTTP server answer now
            // Use the same logic as for the HTTP server here, that is, receive data from the server in a (ring buffer) and parse it
            // We are only interested in few headers:
            typename ToHeaderArray<Headers::ContentType,
                                   Headers::ContentLength,
                                   Headers::TransferEncoding,
                                   Headers::ContentEncoding,
                                   Headers::WWWAuthenticate>::Type answer;
            Container::TranscientVault<ClientBufferSize> recvBuffer;
            ParsingStatus status = ReqLine;
            Code serverAnswer;
            bool endOfHeaders = false;

            // Main loop to receive data
            while(true)
            {
                Error err = socket.recv((char*)recvBuffer.getTail(), recvBuffer.freeSize());
                if (err.isError()) return Code::InternalServerError;
                recvBuffer.stored(err.getCount());

                ROString buffer = recvBuffer.getView<ROString>();
                if (status < HeadersDone)
                {   // Need to make sure we've received a complete line to be able to make progress here
                    if (buffer.Find("\r\n") == buffer.getLength())
                    {
                        if (recvBuffer.freeSize()) continue; // Not enough data for progressing processing, let's try again
                        return Code::ClientRequestError; // Buffer is too small to store the given request
                    }
                }
                switch(status)
                {
                case ReqLine:
                {
                    // Check if we have enough
                    ROString protocol = buffer.splitFrom(" ");
                    if (protocol != "HTTP/1.1" && protocol != "HTTP/1.0") return Code::UnsupportedHTTPVersion;
                    ROString _code = buffer.splitFrom(" ");
                    int code = _code;
                    // Save server code now
                    if (code < 100 || code > 599) return Code::UnsupportedHTTPVersion;

                    serverAnswer = (Code)code;
                    request.callback.serverAnswered(serverAnswer);

                    buffer.splitFrom("\r\n");
                    status = RecvHeaders;
                }
                [[fallthrough]];
                case RecvHeaders:
                {
                    while (status == RecvHeaders) {
                        ROString headerLine, header, value;
                        int pos = buffer.Find("\r\n");
                        if (pos == buffer.getLength()) {
                            // Not enough data to parse a single header, let's refill
                            recvBuffer.drop(buffer.getData());
                            break;
                        }
                        headerLine = buffer.splitAt(pos); buffer.splitAt(2);
                        if (!headerLine) {
                            status = HeadersDone;
                            recvBuffer.drop(buffer.getData()); // Don't drop the buffer, since it'll be required for
                            break;
                        }
                        // Parse the header now
                        if (GenericHeaderParser::parseHeader(headerLine, header) != ParsingError::MoreData)
                            return Code::UnsupportedHTTPVersion;

                        if (GenericHeaderParser::parseValue(headerLine, value) != ParsingError::MoreData)
                            return Code::UnsupportedHTTPVersion;

                        if (request.callback.isInterestedIn(header)) {
                            request.callback.headerReceived(Refl::fromString<Headers>(header).orElse(Headers::Invalid), value);
                        }

                        // Shortcut to avoid having to save the parsed headers in the vault, all other headers are converted to the expected value and don't need specific saving
                        if (header == "Location") {
                            currentURL = value;
                            return serverAnswer; // Will likely loop in the outer function to attempt a redirect
                        }
                        // The code is doing a O(N) search for all the headers we are interested into (those of answer), not all
                        // headers' space (where it could do a O(log N) search). Since the former is much smaller than the latter,
                        // this should still produce a significant speedup despite the algorithmic disadvantage.
                        answer.acceptAndParse(header, value); // Not check for error since if the header isn't required in the answer, it'd return false
                    }
                    if (status == RecvHeaders) continue;
                }
                [[fallthrough]];
                case HeadersDone:
                {
                    // Do the same for any authentication request
                    auto authenticate = answer.getHeader<Headers::WWWAuthenticate>();
                    if (authenticate.getValueElement(0)) {
                        // TODO
                        return serverAnswer;
                    }

                    // Ok, now let's fetch the content, if any
                    auto contentLength = answer.getHeader<Headers::ContentLength>();
                    if (contentLength.getValueElement(0) > 0) {
                        // Need to fetch the given amount of data from the server
                        // Check if we have an encoding and act accordingly here
                        auto contentEncoding = answer.getHeader<Headers::ContentEncoding>();
                        // TODO: Support deflate and gzip encoding here
                        if (contentEncoding.getValueElement(0) != Encoding::identity)
                            return Code::UnsupportedHTTPVersion; // We didn't say we would accept another encoding, so it's an error here

                        size_t totalLen = (size_t)contentLength.getValueElement(0);
                        // Write any pending data first
                        Streams::CachedSocket inStream(*_socket, recvBuffer.getHead(), recvBuffer.getSize());
                        if (!request.callback.dataReceived(inStream, totalLen))
                            return Code::ClientRequestError;
                    }
                    else {
                        // Check if we have a chunked transfer mode and act accordingly here
                        auto transferEncoding = answer.getHeader<Headers::TransferEncoding>();
                        if (transferEncoding.getValueElementsCount() > 1 || transferEncoding.getValueElement(0) != Encoding::chunked)
                            return Code::ClientRequestError; // Combination not supported (but very rare indeed)
                        // Write chunked while decoded
                        Streams::ChunkedInput inStream(*_socket, recvBuffer.getHead(), recvBuffer.getSize());
                        if (!request.callback.dataReceived(inStream))
                            return Code::ClientRequestError;
                    }
                    // Ok, done now
                    return serverAnswer;
                }
                default: return Code::ClientRequestError;
                }
            }


            return Code::Ok;
        }
    };
}

#endif


#endif
