#ifndef hpp_HTTP_Codes_hpp
#define hpp_HTTP_Codes_hpp


// We need reflection code for enum to string conversion
#include "Reflection/AutoEnum.hpp"
// We need a string-view like class for avoiding useless copy here
#include "Strings/ROString.hpp"
// We need compile time string to produce the expected header name
#include "Strings/CTString.hpp"

namespace Protocol::HTTP
{
    /** Status or error code */
    enum class Code
    {
        Invalid                 = -1,

        Continue                = 100,

        Ok                      = 200,
        Created                 = 201,
        Accepted                = 202,
        NonAuthInfo             = 203,
        NoContent               = 204,
        ResetContent            = 205,
        PartialContent          = 206,

        MultipleChoices         = 300,
        MovedForever            = 301,
        MovedTemporarily        = 302,
        SeeOther                = 303,
        NotModified             = 304,
        UseProxy                = 305,
        Unused                  = 306,
        TemporaryRedirect       = 307,

        BadRequest              = 400,
        Unauthorized            = 401,
        PaymentRequired         = 402,
        Forbidden               = 403,
        NotFound                = 404,
        BadMethod               = 405,
        NotAcceptable           = 406,
        ProxyRequired           = 407,
        TimedOut                = 408,
        Conflict                = 409,
        Gone                    = 410,
        LengthRequired          = 411,
        PreconditionFail        = 412,
        EntityTooLarge          = 413,
        URITooLarge             = 414,
        UnsupportedMIME         = 415,
        RequestRange            = 416,
        ExpectationFail         = 417,

        InternalServerError     = 500,
        NotImplemented          = 501,
        BadGateway              = 502,
        Unavailable             = 503,
        GatewayTimedOut         = 504,
        UnsupportedHTTPVersion  = 505,
        ConnectionTimedOut      = 522,
    };

#if defined(MaxSupport)
    constexpr const char * getCodeDescription(Code c) {
        switch(c)
        {
        case Code::Continue                : return "";
        case Code::Ok                      : return "The request processing succeeded";
        case Code::Created                 : return "The request was created";
        case Code::Accepted                : return "The request was accepted";
        case Code::NonAuthInfo             : return "Non authoritative information provided";
        case Code::NoContent               : return "No content found";
        case Code::ResetContent            : return "The server reset the content";
        case Code::PartialContent          : return "The server sent partial content";
        case Code::MultipleChoices         : return "The server gave redirection choices";
        case Code::MovedForever            : return "The content moved permanently";
        case Code::MovedTemporarily        : return "The content moved temporarily";
        case Code::SeeOther                : return "Please see the other url";
        case Code::NotModified             : return "The content wasn't modified";
        case Code::UseProxy                : return "The use of a proxy is not allowed";
        case Code::Unused                  : return "This content is not used";
        case Code::TemporaryRedirect       : return "There is a temporary redirection in place";
        case Code::BadRequest              : return "The server doesn't understand the request";
        case Code::Unauthorized            : return "The server doesn't grant access to this resource";
        case Code::PaymentRequired         : return "Access to this resource requires payment";
        case Code::Forbidden               : return "The server denied access to the content";
        case Code::NotFound                : return "The requested content wasn't found";
        case Code::BadMethod               : return "The used method is not allowed";
        case Code::NotAcceptable           : return "The request is not acceptable";
        case Code::ProxyRequired           : return "Proxy authentication required";
        case Code::TimedOut                : return "The request timed out";
        case Code::Conflict                : return "The server encountered a conflict on the resource";
        case Code::Gone                    : return "The content is gone";
        case Code::LengthRequired          : return "The request length is required";
        case Code::PreconditionFail        : return "The precondition failed";
        case Code::EntityTooLarge          : return "The request entity is too large";
        case Code::URITooLarge             : return "The request URI is too large";
        case Code::UnsupportedMIME         : return "The given media type is not supported";
        case Code::RequestRange            : return "Requested range is not correct";
        case Code::ExpectationFail         : return "Expectation failed";
        case Code::InternalServerError     : return "The server present an internal error";
        case Code::NotImplemented          : return "The requested resource or method isn't implemented";
        case Code::BadGateway              : return "The server use a bad gateway";
        case Code::Unavailable             : return "The service is unavailable";
        case Code::GatewayTimedOut         : return "The gateway timed out";
        case Code::UnsupportedHTTPVersion  : return "The given HTTP version is not supported";
        case Code::ConnectionTimedOut      : return "The connection to the server timed out";
        default: return "";
        }
    }
#else
    inline constexpr const char * getCodeDescription(Code c) { return ""; }
#endif
}

namespace Refl
{
    // Specialize the function here for HTTP status code since there's no logic in the value distribution
    template <> inline constexpr const char * toString(Protocol::HTTP::Code c) {
        using Protocol::HTTP::Code;
        switch(c)
        {
        case Code::Continue                : return "Continue";
        case Code::Ok                      : return "Ok";
        case Code::Created                 : return "Created";
        case Code::Accepted                : return "Accepted";
        case Code::NonAuthInfo             : return "Non Auth Info";
        case Code::NoContent               : return "No Content";
        case Code::ResetContent            : return "Reset Content";
        case Code::PartialContent          : return "Partial Content";
        case Code::MultipleChoices         : return "Multiple Choices";
        case Code::MovedForever            : return "Moved Forever";
        case Code::MovedTemporarily        : return "Moved Temporarily";
        case Code::SeeOther                : return "See Other";
        case Code::NotModified             : return "Not Modified";
        case Code::UseProxy                : return "Use Proxy";
        case Code::Unused                  : return "Unused";
        case Code::TemporaryRedirect       : return "Temporary Redirect";
        case Code::BadRequest              : return "Bad Request";
        case Code::Unauthorized            : return "Unauthorized";
        case Code::PaymentRequired         : return "Payment Required";
        case Code::Forbidden               : return "Forbidden";
        case Code::NotFound                : return "Not Found";
        case Code::BadMethod               : return "Bad Method";
        case Code::NotAcceptable           : return "Not Acceptable";
        case Code::ProxyRequired           : return "Proxy Required";
        case Code::TimedOut                : return "Timed Out";
        case Code::Conflict                : return "Conflict";
        case Code::Gone                    : return "Gone";
        case Code::LengthRequired          : return "Length Required";
        case Code::PreconditionFail        : return "Precondition Fail";
        case Code::EntityTooLarge          : return "Entity Too Large";
        case Code::URITooLarge             : return "URI Too Large";
        case Code::UnsupportedMIME         : return "Unsupported MIME";
        case Code::RequestRange            : return "Request Range";
        case Code::ExpectationFail         : return "Expectation Fail";
        case Code::InternalServerError     : return "Internal Server Error";
        case Code::NotImplemented          : return "Not Implemented";
        case Code::BadGateway              : return "Bad Gateway";
        case Code::Unavailable             : return "Unavailable";
        case Code::GatewayTimedOut         : return "Gateway Timed Out";
        case Code::UnsupportedHTTPVersion  : return "Unsupported HTTP Version";
        case Code::ConnectionTimedOut      : return "Connection Timed Out";
        default: return "";
        }
    }


}

#endif
