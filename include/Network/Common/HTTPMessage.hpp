#ifndef hpp_HTTPMessage_hpp
#define hpp_HTTPMessage_hpp

// We need headers array too
#include "HeadersArray.hpp"
// We need status code too
#include "Protocol/HTTP/Codes.hpp"
// We need socket code too
#include "Network/Socket.hpp"


// Common code shared by HTTP server and client to avoid binary size deduplication
namespace Network::Common::HTTP
{
    using namespace Protocol::HTTP;

    static constexpr const char EOM[] = "\r\n\r\n";

    /** A client answer structure.
        This is a convenient, template type, made to build an answer for an HTTP request.
        There are 3 different possible answer type supported by this library:
        1. A DIY answer, where you take over the client's socket, and send whatever you want (likely breaking HTTP protocol) - Not recommended
        2. A simple answer, with basic headers (like Content-Type) and a fixed string as output.
        3. A more complex answer with basic headers and you want to have control over the output stream (in that case, you'll give an input stream to the library to consume)

        It's templated over the list of headers you intend to use in the answer (it's a statically built for saving both binary space, memory and logic).
        In all cases, Content-Length is commputed by the library. Transfer-Encoding can be set up by you or the library depending on the stream itself
        @param getContentFunc   A content function that returns a stream that can be used by the client to send the answer. If not provided, defaults to a simple string
        @param headersToSend    A list of headers you are expecting to answer */
    template< typename Child, typename Socket, Headers ... headersToSend>
    struct CommonHeader
    {
        // Construct the answer header array
        typedef typename ToAnswerHeader<headersToSend...>::Type ExpectedHeaderArray;
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

#if MinimizeStackSize == 1
        bool sendHeaders(Socket & socket) { return headers.sendHeaders(socket); }
#else
        bool sendHeaders(Socket & socket, Container::TrackedBuffer & buffer)
        {
            if (!headers.sendHeaders(buffer)) return false;
            return socket.send(buffer.buffer, buffer.used) == buffer.used;
        }
#endif
        CommonHeader(Code code = Code::Invalid) : replyCode(code) {}
    };

    /** Useful helper to map extension to a MIME type */
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

    /** The absolute minimum for sending the Content-Length header helper (without using sprintf or itoa) */
    static bool sendSize(BaseSocket & socket, std::size_t length)
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
}


#endif
