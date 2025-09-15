#ifndef hpp_HTTPDConfig_hpp
#define hpp_HTTPDConfig_hpp

// Configuration is done via macros
#if defined(ESP_PLATFORM)
  #include <sdkconfig.h>
  #include CONFIG_ESP_EHTTPD_FORCEINCLUDE
#endif


/** Client transcient-vault buffer size. Must be a power of 2.
    This is used to store the transcient data received from the client (like headers and content of an HTTP request)
    and also keeps important information.
    Default: 1024 */
#define ClientBufferSize      CONFIG_ESP_EHTTPD_CLIENT_BUFFER_SIZE


/** Enable SSL/TLS code for server.
    It's quite rare that an embedded server requires TLS, since certificate management is almost impossible to ensure
    on an embedded system.

    Default: 0 */
#define UseTLSServer          CONFIG_ESP_EHTTPD_TLS_SERVER

/** Enable SSL/TLS code for client.
    Use a client that accept to connect to any HTTPS website.

    Default: 0 */
#define UseTLSClient          CONFIG_ESP_EHTTPD_TLS_CLIENT

/** Build a HTTP client too
    A HTTP client is very similar to a server for message parsing, so it makes senses to also
    build a HTTP client to avoid wasting another HTTP client library code (and parser) in your binary

    Default: 0 */
#define BuildClient           CONFIG_ESP_EHTTPD_CLIENT_ENABLED

/** Prefer more code to less memory usage
    If this parameter is set, the code will try to limit using stack and/or heap space to create HTTP
    protocol's buffers, and instead will directly write to the socket (thus deporting the work to the network stack)

    Default: 1 */
#define MinimizeStackSize     CONFIG_ESP_EHTTPD_MINIMIZE_STACK_SIZE


/** Enable max compatibility support with RFC2616 (HTTP) standard.
    Allows to support more features in the HTTP server.
    This increases the binary size
    Default: 0 */
#define MaxSupport            CONFIG_ESP_EHTTPD_MAX_SUPPORT

#if UseTLSServer == 1 || UseTLSClient == 1
  #define UseTLS 1
#else
  #define UseTLS 0
#endif

#endif
