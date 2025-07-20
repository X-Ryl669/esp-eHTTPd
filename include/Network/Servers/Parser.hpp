#ifndef hpp_HTTP_Parser_hpp
#define hpp_HTTP_Parser_hpp

// We need methods and request line parsing
#include "../../Protocol/HTTP/RequestLine.hpp"

namespace Network::Servers::HTTP
{
    using Protocol::HTTP::RequestLine;

    /** The HTTP parser is following RFC2616 (more or less, see doc/Compliance.md for opinion based implementation choices)
        It's called by a client for processing a single HTTP request, it's stateless */
    struct Parser
    {
        /** The received request line */
        RequestLine request;
        /** The expected header map. Unexpected headers are ignored */
     //   RequestHeaderLines & headers;


    };
}

#endif
