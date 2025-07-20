#ifndef hpp_HTTP_Methods_hpp
#define hpp_HTTP_Methods_hpp

// We need reflection code for enum to string conversion
#include "Reflection/AutoEnum.hpp"
// We need a string-view like class for avoiding useless copy here
#include "Strings/ROString.hpp"
// We need compile time string to produce the expected header name
#include "Strings/CTString.hpp"

// Simple macro to avoid splattering the code with #if/#endif everywhere
#define CONCAT2(A, B) A##B
#define CONCAT2_DEFERRED(A, B) CONCAT2(A, B)
#define IF_0(true_case, false_case) false_case,
#define IF_1(true_case, false_case) true_case,
#define IF(condition, true_case, false_case) CONCAT2_DEFERRED(IF_, condition)(true_case, false_case)

#define MaxSupport 1

namespace Protocol::HTTP
{
    /** Inject the reflection tools to convert to/from enumerations */
    using Refl::toString;
    using Refl::fromString;

    typedef signed char int8;


    /** The protocol version. This MUST be sorted (except for Invalid and all) */
    enum class Version : int8
    {
        Invalid = -1,
        HTTP1_0 = 0,
        HTTP1_1 = 1,
    };

    /** The supported HTTP methods. This MUST be sorted (except for Invalid and all) */
    enum class Method : int8
    {
        Invalid = -1,
        DELETE  = 0,
        GET     = 1,
        HEAD    = 2,
        OPTIONS = 3,
        POST    = 4,
        PUT     = 5,
    };

    /** The method mask to use in routes */
    struct MethodsMask
    {
        uint32 mask;

        static constexpr inline uint32 makeMask(Method method) { return method >= Method::DELETE ? (1U<<(uint32)method) : 0; }
        /** This will hopefully break compilation if used with no method. You need one method at least here */
        template <typename ... Methods> constexpr MethodsMask(Methods ... methods) : mask(0) { mask = (makeMask(methods) | ...); }
    };


    /////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////  Headers below  ////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////

    /** The important HTTP headers used in this library. All other headers are accessible via a callback but those are converted to an enum value since it's required for any request. This MUST be sorted (except for Invalid and all) */
    enum class Headers : int8
    {
        Invalid = -1,
        Accept = 0,
        IF(MaxSupport, AcceptCharset, )
        IF(MaxSupport, AcceptDatetime, )
        AcceptEncoding,
        AcceptLanguage,
        IF(MaxSupport, AcceptPatch, )
        AcceptRanges,
        IF(MaxSupport, AccessControlAllowCredentials, )
        IF(MaxSupport, AccessControlAllowHeaders, )
        IF(MaxSupport, AccessControlAllowMethods, )
        AccessControlAllowOrigin,
        IF(MaxSupport, AccessControlExposeHeaders, )
        IF(MaxSupport, AccessControlMaxAge, )
        IF(MaxSupport, AccessControlRequestMethod, )
        IF(MaxSupport, Allow, )
        Authorization,
        CacheControl,
        Connection,
        ContentDisposition,
        ContentEncoding,
        ContentLanguage,
        ContentLength,
        IF(MaxSupport, ContentLocation, )
        ContentRange,
        ContentType,
        Cookie,
        Date,
        IF(MaxSupport, ETag, )
        IF(MaxSupport, Expect, )
        Expires,
        IF(MaxSupport, Forwarded, )
        IF(MaxSupport, From, )
        Host,
        IF(MaxSupport, IfMatch, )
        IF(MaxSupport, IfModifiedSince, )
        IF(MaxSupport, IfNoneMatch, )
        IF(MaxSupport, IfRange, )
        IF(MaxSupport, IfUnmodifiedSince, )
        LastModified,
        IF(MaxSupport, Link, )
        Location,
        IF(MaxSupport, MaxForwards, )
        Origin,
        Pragma,
        IF(MaxSupport, Prefer, )
        IF(MaxSupport, ProxyAuthorization, )
        Range,
        Referer,
        Server,
        SetCookie,
        IF(MaxSupport, StrictTransportSecurity, )
        TE,
        IF(MaxSupport, Trailer, )
        TransferEncoding,
        Upgrade,
        UserAgent,
        IF(MaxSupport, Via, )
        WWWAuthenticate,
        IF(MaxSupport, XForwardedFor, )
    };

    /** The important HTTP headers used in this library. All other headers are accessible via a callback but those are converted to an enum value since it's required for any request. This MUST be sorted (except for Invalid and all) */
    enum class RequestHeaders : int8
    {
        Accept = (int8)Headers::Accept,
        AcceptCharset = (int8)Headers::AcceptCharset,
        IF(MaxSupport, AcceptDatetime = (int8)Headers::AcceptDatetime, )
        AcceptEncoding = (int8)Headers::AcceptEncoding,
        AcceptLanguage = (int8)Headers::AcceptLanguage,
        IF(MaxSupport, AccessControlRequestMethod = (int8)Headers::AccessControlRequestMethod, )
        Authorization = (int8)Headers::Authorization,
        CacheControl = (int8)Headers::CacheControl,
        Connection = (int8)Headers::Connection,
        ContentEncoding = (int8)Headers::ContentEncoding,
        ContentLength = (int8)Headers::ContentLength,
        ContentType = (int8)Headers::ContentType,
        Cookie = (int8)Headers::Cookie,
        Date = (int8)Headers::Date,
        IF(MaxSupport, Expect = (int8)Headers::Expect, )
        IF(MaxSupport, Forwarded = (int8)Headers::Forwarded, )
        IF(MaxSupport, From = (int8)Headers::From, )
        Host = (int8)Headers::Host,
        IF(MaxSupport, IfMatch = (int8)Headers::IfMatch, )
        IF(MaxSupport, IfModifiedSince = (int8)Headers::IfModifiedSince, )
        IF(MaxSupport, IfNoneMatch = (int8)Headers::IfNoneMatch, )
        IF(MaxSupport, IfRange = (int8)Headers::IfRange, )
        IF(MaxSupport, IfUnmodifiedSince = (int8)Headers::IfUnmodifiedSince, )
        IF(MaxSupport, MaxForwards = (int8)Headers::MaxForwards, )
        Origin = (int8)Headers::Origin,
        IF(MaxSupport, Prefer = (int8)Headers::Prefer, )
        IF(MaxSupport, ProxyAuthorization = (int8)Headers::ProxyAuthorization, )
        Range = (int8)Headers::Range,
        Referer = (int8)Headers::Referer,
        TE = (int8)Headers::TE,
        IF(MaxSupport, Trailer = (int8)Headers::Trailer, )
        TransferEncoding = (int8)Headers::TransferEncoding,
        Upgrade = (int8)Headers::Upgrade,
        UserAgent = (int8)Headers::UserAgent,
        IF(MaxSupport, Via = (int8)Headers::Via, )
        IF(MaxSupport, XForwardedFor = (int8)Headers::XForwardedFor, )
    };

    /** The usual response headers in HTTP. This MUST be sorted (except for Invalid and all) */
    enum class ResponseHeaders : int8
    {
        AccessControlAllowOrigin = (int8)Headers::AccessControlAllowOrigin,
        IF(MaxSupport, AccessControlAllowCredentials = (int8)Headers::AccessControlAllowCredentials, )
        IF(MaxSupport, AccessControlExposeHeaders = (int8)Headers::AccessControlExposeHeaders, )
        IF(MaxSupport, AccessControlMaxAge = (int8)Headers::AccessControlMaxAge, )
        IF(MaxSupport, AccessControlAllowMethods = (int8)Headers::AccessControlAllowMethods, )
        IF(MaxSupport, AccessControlAllowHeaders = (int8)Headers::AccessControlAllowHeaders, )
        IF(MaxSupport, AcceptPatch = (int8)Headers::AcceptPatch, )
        AcceptRanges = (int8)Headers::AcceptRanges,
        IF(MaxSupport, Allow = (int8)Headers::Allow, )
        CacheControl = (int8)Headers::CacheControl,
        Connection = (int8)Headers::Connection,
        ContentDisposition = (int8)Headers::ContentDisposition,
        ContentEncoding = (int8)Headers::ContentEncoding,
        ContentLanguage = (int8)Headers::ContentLanguage,
        ContentLength = (int8)Headers::ContentLength,
        IF(MaxSupport, ContentLocation = (int8)Headers::ContentLocation, )
        ContentRange = (int8)Headers::ContentRange,
        ContentType = (int8)Headers::ContentType,
        Date = (int8)Headers::Date,
        IF(MaxSupport, ETag = (int8)Headers::ETag, ) // This one will break the parser
        Expires = (int8)Headers::Expires,
        LastModified = (int8)Headers::LastModified,
        IF(MaxSupport, Link = (int8)Headers::Link, )
        Location = (int8)Headers::Location,
        Pragma = (int8)Headers::Pragma,
        Server = (int8)Headers::Server,
        SetCookie = (int8)Headers::SetCookie,
        IF(MaxSupport, StrictTransportSecurity = (int8)Headers::StrictTransportSecurity, )
        IF(MaxSupport, Trailer = (int8)Headers::Trailer, )
        TransferEncoding = (int8)Headers::TransferEncoding,
        Upgrade = (int8)Headers::Upgrade,
        WWWAuthenticate = (int8)Headers::WWWAuthenticate,
    };

    /** A compile time processing of the enumeration name to make an enumeration string representation that suit HTTP standard of header naming */
    template <auto name>
    constexpr size_t countUppercase() { size_t n = 0; for (size_t i = 0; name[i]; i++) if (name[i] >= 'A' && name[i] <= 'Z') ++n; return n; }

    /** This converts, at compile time, the enum value like "UserAgent" to HTTP expected format for headers: "User-Agent" */
    template <auto name>
    constexpr auto makeHTTPHeader()
    {
        if constexpr (name.size < 5) return name; // Name smaller than 4 char are returned as is
        else
        {
            // Count the number of uppercase letter
            constexpr size_t upCount = countUppercase<name>();
            char output[name.size + upCount - 1] = { name.data[0], 0};
            for (size_t i = 1, j = 1; i < name.size; i++)
            {
                if (name.data[i] >= 'A' && name.data[i] <= 'Z') output[j++] = '-';
                output[j++] = name.data[i];
            }
            return CompileTime::str(output);
        }
    }
    // Exception below
    template <> constexpr auto makeHTTPHeader<Refl::enum_raw_name_only_str<Headers, (int)Headers::WWWAuthenticate>()>()  { return CompileTime::str("WWW-Authenticate"); }



    /////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////// MIME Type below ////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////


#if defined(MaxSupport)
    /** This is used in accept headers. This MUST be sorted (except for Invalid and all) */
    enum class MediaType
    {
        Invalid = -1,
        application = 0,
        audio       = 1,
        font        = 2,
        image       = 3,
        model       = 4,
        multipart   = 5,  // Special case for this one, it's not expected in the Accept header but in the Content-Type one
        text        = 6,
        video       = 7,
    };

    /** The subtypes for: Application. This MUST be sorted (except for Invalid and all) */
    enum class ApplicationType
    {
        Invalid = -1,
        ecmascript             = 0, // Legacy
        javascript,                 // Legacy
        json,
        octetStream,
        pdf,
        xWwwFormUrlencoded,
        xhtml__xml,
        xml,
        zip,
    };

    /** The subtypes for: Audio. This MUST be sorted (except for Invalid and all) */
    enum class AudioType
    {
        Invalid        = -1,
        mpeg                    = 0,
        vorbis,
    };

    /** The subtypes for: Font. This MUST be sorted (except for Invalid and all) */
    enum class FontType
    {
        Invalid        = -1,
        otf                    = 0,
        ttf,
        woff,
    };

    /** The subtypes for: Image. This MUST be sorted (except for Invalid and all) */
    enum class ImageType
    {
        Invalid       = -1,
        apng                   = 0,
        avif,
        gif,
        jpeg,
        png,
        svg__xml,
        vnd___microsoft___icon,         // Yes, sorry.
        webp,
    };

    /** The subtypes for: Model. This MUST be sorted (except for Invalid and all) */
    enum class ModelType
    {
        Invalid       = -1,
        Tmf                    = 0, // Invalid should be 3mf but this isn't supported by C++ charset
        vrml,
    };

    /** The subtypes for: Multipart. This MUST be sorted (except for Invalid and all) */
    enum class MultipartType
    {
        Invalid   = -1,
        formData               = 0,
        byteranges,
    };

    /** The subtypes for: Text. This MUST be sorted (except for Invalid and all) */
    enum class TextType
    {
        Invalid   = -1,
        css               = 0,
        csv,
        html,
        javascript,
        plain,
    };
#endif
    /** This is used in accept headers and other MIME type related header. This MUST be sorted (except for Invalid and all) */
    enum class MIMEType : int8
    {
        Invalid = -1,
        all = 0,

        application_all,
        application_ecmascript,
        application_javascript,
        application_json,
        application_octetStream,
        application_pdf,
        application_xWwwFormUrlencoded,
        application_xhtml__xml,
        application_xml,
        application_zip,

        audio_all,
        audio_mpeg,
        audio_vorbis,

        font_all,
        font_otf,
        font_ttf,
        font_woff,

        image_all,
        image_apng,
        image_avif,
        image_gif,
        image_jpeg,
        image_png,
        image_svg__xml,
        image_vnd___microsoft___icon,
        image_webp,

        model_all,
        model_3mf,
        model_vrml,

        multipart_formData,
        multipart_byteranges,

        text_all,
        text_css,
        text_csv,
        text_html,
        text_javascript,
        text_plain,
    };

    struct UnexpectedChars { size_t plus = 0, dot = 0, slash = 0, uppercase = 0; };
    template <auto name>
    constexpr UnexpectedChars countSpecialChars() {
        UnexpectedChars c;
        for (size_t i = 1; name[i]; i++) { // Ignore first char in all cases
            if (name[i] == '_') { // One underscore for a slash, two for a plus, three for a dot
                if (name[i+1] && name[i+1] == '_') {
                    if (name[i+2] && name[i+2] == '_') {
                        c.dot++;
                        i++;
                    } else c.plus++;
                    i++;
                } else c.slash++;
            } else if (name[i] >= 'A' && name[i] <= 'Z') c.uppercase++;
        }
        return c;
    }


    /** This converts, at compile time, the enum value like "octetStreal", "svg_xml" to MIME expected format: "octet-stream", "svg+xml" */
    template <auto name>
    constexpr auto makeMIMEHeader()
    {
        // Count the number of uppercase letter
        constexpr UnexpectedChars c = countSpecialChars<name>();
        char output[name.size + c.uppercase - c.plus - 2 * c.dot] = { name.data[0], 0};
        for (size_t i = 1, j = 1; i < name.size; i++)
        {
            if (name.data[i] >= 'A' && name.data[i] <= 'Z') {
                output[j++] = '-';
                output[j++] = name.data[i] - 'A' + 'a';
            }
            else if (name.data[i] == '_') {
                if (i+1 < name.size && name.data[i+1] == '_') {
                    if (i+2 < name.size && name.data[i+2] == '_') {
                        output[j++] = '.';
                        i+=2;
                    } else {
                        output[j++] = '+';
                        i++;
                    }
                } else output[j++] = '/';
            } else output[j++] = name.data[i];
        }
        return CompileTime::str(output);
    }
    // Exceptions below
    template <> constexpr auto makeMIMEHeader<Refl::enum_raw_name_only_str<MIMEType, (int)MIMEType::all>()>()  { return CompileTime::str("*/*"); }
    template <> constexpr auto makeMIMEHeader<Refl::enum_raw_name_only_str<MIMEType, (int)MIMEType::application_all>()>()  { return CompileTime::str("application/*"); }
    template <> constexpr auto makeMIMEHeader<Refl::enum_raw_name_only_str<MIMEType, (int)MIMEType::audio_all>()>()  { return CompileTime::str("audio/*"); }
    template <> constexpr auto makeMIMEHeader<Refl::enum_raw_name_only_str<MIMEType, (int)MIMEType::font_all>()>()  { return CompileTime::str("font/*"); }
    template <> constexpr auto makeMIMEHeader<Refl::enum_raw_name_only_str<MIMEType, (int)MIMEType::image_all>()>()  { return CompileTime::str("image/*"); }
    template <> constexpr auto makeMIMEHeader<Refl::enum_raw_name_only_str<MIMEType, (int)MIMEType::model_all>()>()  { return CompileTime::str("model/*"); }
    template <> constexpr auto makeMIMEHeader<Refl::enum_raw_name_only_str<MIMEType, (int)MIMEType::text_all>()>()  { return CompileTime::str("text/*"); }

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////  Charset below  ////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////

    /** Accept-Charset header values. This MUST be sorted (except for Invalid and all) */
    enum class Charset : char
    {
        Invalid      = -1,
        ISO_8859_1,
        ISO_8859_2,
        ISO_8859_3,
        ISO_8859_4,
        ISO_8859_5,
        ISO_8859_6,
        ISO_8859_7,
        ISO_8859_8,
        ISO_8859_9,
        ISO_8859_10,
        ISO_8859_11,
        ISO_8859_12,
        ISO_8859_13,
        ISO_8859_14,
        ISO_8859_15,
        ISO_8859_16,
        ISO_8859_x,

        us_ascii,

        utf_8,
        utf_16,
        utf_32,
    };

    /** This converts, at compile time, the enum value like "us_ascii" to HTTP expected format for charset: "us-ascii" */
    template <auto name>
    constexpr auto makeHTTPCharset()
    {
        char output[name.size] = { name.data[0], 0};
        for (size_t i = 1; i < name.size; i++)
        {
            if (name.data[i] == '_') output[i] = '-';
            else output[i] = name.data[i];
        }
        return CompileTime::str(output);
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////// Encoding  below ////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////

    /** Accept-Encoding / Content-Encoding headers. This MUST be sorted (except for Invalid and all) */
    enum class Encoding : char
    {
        Invalid = -1,
        all,
        br,
        chunked,
        compress,
        deflate,
        gzip,
        identity,
    };

    // Reuse HTTP charset rules here to allow exception for all
    template <> constexpr auto makeHTTPCharset<Refl::enum_raw_name_only_str<Encoding, (int)Encoding::all>()>()  { return CompileTime::str("*"); }


    /////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////// Languages below ////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////

    /** Accept-Language headers. This MUST be sorted (except for Invalid and all) */
    enum class Language : int8
    {
        Invalid = -1,
        all,
        IF(MaxSupport, af,)
        IF(MaxSupport, am,) IF(MaxSupport, ar,) IF(MaxSupport, az,)
        IF(MaxSupport, be,) IF(MaxSupport, bg,) IF(MaxSupport, bn,) IF(MaxSupport, bs,)
        IF(MaxSupport, ca,) IF(MaxSupport, co,) IF(MaxSupport, cs,) IF(MaxSupport, cy,)
        IF(MaxSupport, da,) IF(MaxSupport, de,)
        IF(MaxSupport, el,)
        en,
        IF(MaxSupport, eo,) IF(MaxSupport, es,) IF(MaxSupport, et,) IF(MaxSupport, eu,)
        IF(MaxSupport, fa,) IF(MaxSupport, fi,) IF(MaxSupport, fr,) IF(MaxSupport, fy,)
        IF(MaxSupport, ga,) IF(MaxSupport, gd,) IF(MaxSupport, gl,) IF(MaxSupport, gu,)
        IF(MaxSupport, ha,) IF(MaxSupport, he,) IF(MaxSupport, hi,) IF(MaxSupport, hr,) IF(MaxSupport, hu,) IF(MaxSupport, hy,)
        IF(MaxSupport, id,) IF(MaxSupport, is,) IF(MaxSupport, it,)
        IF(MaxSupport, ja,) IF(MaxSupport, jv,)
        IF(MaxSupport, ka,) IF(MaxSupport, kk,) IF(MaxSupport, km,) IF(MaxSupport, kn,) IF(MaxSupport, ko,) IF(MaxSupport, kr,) IF(MaxSupport, ku,) IF(MaxSupport, ky,)
        IF(MaxSupport, lb,) IF(MaxSupport, lt,) IF(MaxSupport, lv,)
        IF(MaxSupport, me,) IF(MaxSupport, mg,) IF(MaxSupport, mi,) IF(MaxSupport, mk,) IF(MaxSupport, ml,) IF(MaxSupport, mn,) IF(MaxSupport, mr,) IF(MaxSupport, ms,) IF(MaxSupport, mt,) IF(MaxSupport, my,)
        IF(MaxSupport, nb,) IF(MaxSupport, ne,) IF(MaxSupport, nl,) IF(MaxSupport, no,)
        IF(MaxSupport, pa,) IF(MaxSupport, pl,) IF(MaxSupport, ps,) IF(MaxSupport, pt,)
        IF(MaxSupport, ro,) IF(MaxSupport, ru,)
        IF(MaxSupport, sd,) IF(MaxSupport, si,) IF(MaxSupport, sk,) IF(MaxSupport, sl,) IF(MaxSupport, sm,) IF(MaxSupport, sn,) IF(MaxSupport, so,) IF(MaxSupport, sq,) IF(MaxSupport, sr,) IF(MaxSupport, st,) IF(MaxSupport, su,) IF(MaxSupport, sv,) IF(MaxSupport, sw,)
        IF(MaxSupport, ta,) IF(MaxSupport, te,) IF(MaxSupport, tg,) IF(MaxSupport, th,) IF(MaxSupport, tr,) IF(MaxSupport, tt,)
        IF(MaxSupport, uk,) IF(MaxSupport, ur,) IF(MaxSupport, uz,)
        IF(MaxSupport, vi,)
        IF(MaxSupport, xh,)
        IF(MaxSupport, yi,) IF(MaxSupport, yo,)
        IF(MaxSupport, zh,) IF(MaxSupport, zu,)
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////// Cache control below //////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////

    /** Cache-Control headers. This MUST be sorted (except for Invalid and all) */
    enum class CacheControl : int8
    {
        Invalid = -1,
        max_age,
        max_stale,
        min_fresh,
        must_revalidate,
        no_cache,
        no_transform,
        no_store,
        only_if_cached,
    };

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////// Connection below //////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////

    /** Connection headers. This MUST be sorted (except for Invalid and all) */
    enum Connection : int8
    {
        Invalid = -1,
        close,
        keep_alive,
        upgrade,
    };

}

namespace Refl
{
    template <> constexpr bool isCaseSensitive<Protocol::HTTP::Method> = false;
    template <> constexpr bool isCaseSensitive<Protocol::HTTP::Headers> = false;
    template <> constexpr bool isCaseSensitive<Protocol::HTTP::Charset> = false;
    template <> constexpr bool isCaseSensitive<Protocol::HTTP::Language> = false;


    template <> constexpr bool isSorted<Protocol::HTTP::Method> = true;
    template <> constexpr bool isSorted<Protocol::HTTP::Headers> = true;
    template <> constexpr bool isSorted<Protocol::HTTP::Charset> = true;
    template <> constexpr bool isSorted<Protocol::HTTP::Encoding> = true;
    template <> constexpr bool isSorted<Protocol::HTTP::CacheControl> = true;
    template <> constexpr bool isSorted<Protocol::HTTP::Connection> = true;
    template <> constexpr bool isSorted<Protocol::HTTP::Language> = true;
#if defined(MaxSupport)
    template <> constexpr bool isSorted<Protocol::HTTP::ApplicationType> = true;
    template <> constexpr bool isSorted<Protocol::HTTP::AudioType> = true;
    template <> constexpr bool isSorted<Protocol::HTTP::FontType> = true;
    template <> constexpr bool isSorted<Protocol::HTTP::ImageType> = true;
    template <> constexpr bool isSorted<Protocol::HTTP::ModelType> = true;
    template <> constexpr bool isSorted<Protocol::HTTP::MultipartType> = true;
    template <> constexpr bool isSorted<Protocol::HTTP::TextType> = true;
#endif
    template <> constexpr bool isSorted<Protocol::HTTP::MIMEType> = true;

#if defined(UseHashForReflection)
    template <> constexpr bool useHash<Protocol::HTTP::Charset> = true;
    template <> constexpr bool useHash<Protocol::HTTP::Encoding> = true;
    template <> constexpr bool useHash<Protocol::HTTP::CacheControl> = true;
    template <> constexpr bool useHash<Protocol::HTTP::Connection> = true;
    template <> constexpr bool useHash<Protocol::HTTP::Language> = true;
  #if defined(MaxSupport)
    template <> constexpr bool useHash<Protocol::HTTP::ApplicationType> = true;
    template <> constexpr bool useHash<Protocol::HTTP::AudioType> = true;
    template <> constexpr bool useHash<Protocol::HTTP::FontType> = true;
    template <> constexpr bool useHash<Protocol::HTTP::ImageType> = true;
    template <> constexpr bool useHash<Protocol::HTTP::ModelType> = true;
    template <> constexpr bool useHash<Protocol::HTTP::MultipartType> = true;
    template <> constexpr bool useHash<Protocol::HTTP::TextType> = true;
  #endif
    template <> constexpr bool useHash<Protocol::HTTP::MIMEType> = true;
#endif

    namespace Details { template <Enum E, std::size_t ... i > struct ReflectHTTPHeader { static constexpr std::array<const char*, sizeof...(i)> values = {CompileTime::str_ref<Protocol::HTTP::makeHTTPHeader<enum_raw_name_only_str<E, (int)i>()>()>{}.data...}; }; }
#if defined(MaxSupport)
    namespace Details { template <Enum E, std::size_t ... i > struct ReflectMIMEHeader { static constexpr std::array<const char*, sizeof...(i)> values = {CompileTime::str_ref<Protocol::HTTP::makeMIMEHeader<enum_raw_name_only_str<E, (int)i>()>()>{}.data...}; }; }
#endif
    namespace Details { template <Enum E, std::size_t ... i > struct ReflectHTTPCharset { static constexpr std::array<const char*, sizeof...(i)> values = {CompileTime::str_ref<Protocol::HTTP::makeHTTPCharset<enum_raw_name_only_str<E, (int)i>()>()>{}.data...}; }; }

    template <>
    constexpr inline auto & _supports<Protocol::HTTP::Headers>()
    {
        constexpr auto maxV = find_max_value<Protocol::HTTP::Headers, 0>();
        return *[]<std::size_t ... i>(std::index_sequence<i...>) { return &Details::ReflectHTTPHeader<Protocol::HTTP::Headers, i...>::values; }(std::make_index_sequence<maxV+1>{});
    }
#if defined(MaxSupport)
    template <> constexpr inline auto & _supports<Protocol::HTTP::ApplicationType>()
    {
        constexpr auto maxV = find_max_value<Protocol::HTTP::ApplicationType, 0>();
        return *[]<std::size_t ... i>(std::index_sequence<i...>) { return &Details::ReflectMIMEHeader<Protocol::HTTP::ApplicationType, i...>::values; }(std::make_index_sequence<maxV+1>{});
    }
    template <> constexpr inline auto & _supports<Protocol::HTTP::ImageType>()
    {
        constexpr auto maxV = find_max_value<Protocol::HTTP::ImageType, 0>();
        return *[]<std::size_t ... i>(std::index_sequence<i...>) { return &Details::ReflectMIMEHeader<Protocol::HTTP::ImageType, i...>::values; }(std::make_index_sequence<maxV+1>{});
    }
    template <> constexpr inline auto & _supports<Protocol::HTTP::ModelType>()
    {
        constexpr auto maxV = find_max_value<Protocol::HTTP::ModelType, 0>();
        return *[]<std::size_t ... i>(std::index_sequence<i...>) { return &Details::ReflectMIMEHeader<Protocol::HTTP::ModelType, i...>::values; }(std::make_index_sequence<maxV+1>{});
    }
    template <> constexpr inline auto & _supports<Protocol::HTTP::MultipartType>()
    {
        constexpr auto maxV = find_max_value<Protocol::HTTP::MultipartType, 0>();
        return *[]<std::size_t ... i>(std::index_sequence<i...>) { return &Details::ReflectMIMEHeader<Protocol::HTTP::MultipartType, i...>::values; }(std::make_index_sequence<maxV+1>{});
    }
    template <> constexpr inline auto & _supports<Protocol::HTTP::TextType>()
    {
        constexpr auto maxV = find_max_value<Protocol::HTTP::TextType, 0>();
        return *[]<std::size_t ... i>(std::index_sequence<i...>) { return &Details::ReflectMIMEHeader<Protocol::HTTP::TextType, i...>::values; }(std::make_index_sequence<maxV+1>{});
    }
#endif
    template <> constexpr inline auto & _supports<Protocol::HTTP::MIMEType>()
    {
        constexpr auto maxV = find_max_value<Protocol::HTTP::MIMEType, 0>();
        return *[]<std::size_t ... i>(std::index_sequence<i...>) { return &Details::ReflectMIMEHeader<Protocol::HTTP::MIMEType, i...>::values; }(std::make_index_sequence<maxV+1>{});
    }

    template <>
    constexpr inline auto & _supports<Protocol::HTTP::Charset>()
    {
        constexpr auto maxV = find_max_value<Protocol::HTTP::Charset, 0>();
        return *[]<std::size_t ... i>(std::index_sequence<i...>) { return &Details::ReflectHTTPCharset<Protocol::HTTP::Charset, i...>::values; }(std::make_index_sequence<maxV+1>{});
    }

    template <>
    constexpr inline auto & _supports<Protocol::HTTP::Encoding>()
    {
        constexpr auto maxV = find_max_value<Protocol::HTTP::Encoding, 0>();
        return *[]<std::size_t ... i>(std::index_sequence<i...>) { return &Details::ReflectHTTPCharset<Protocol::HTTP::Encoding, i...>::values; }(std::make_index_sequence<maxV+1>{});
    }
    template <>
    constexpr inline auto & _supports<Protocol::HTTP::Language>()
    {
        constexpr auto maxV = find_max_value<Protocol::HTTP::Language, 0>();
        return *[]<std::size_t ... i>(std::index_sequence<i...>) { return &Details::ReflectHTTPCharset<Protocol::HTTP::Language, i...>::values; }(std::make_index_sequence<maxV+1>{});
    }
    template <>
    constexpr inline auto & _supports<Protocol::HTTP::CacheControl>()
    {
        constexpr auto maxV = find_max_value<Protocol::HTTP::CacheControl, 0>();
        return *[]<std::size_t ... i>(std::index_sequence<i...>) { return &Details::ReflectHTTPCharset<Protocol::HTTP::CacheControl, i...>::values; }(std::make_index_sequence<maxV+1>{});
    }
    template <>
    constexpr inline auto & _supports<Protocol::HTTP::Connection>()
    {
        constexpr auto maxV = find_max_value<Protocol::HTTP::Connection, 0>();
        return *[]<std::size_t ... i>(std::index_sequence<i...>) { return &Details::ReflectHTTPCharset<Protocol::HTTP::Connection, i...>::values; }(std::make_index_sequence<maxV+1>{});
    }


}

#endif
