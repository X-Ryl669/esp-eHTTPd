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
#define UseTLSServer          CONFIG_ESP_EHTTPD_TLS_ENABLE


/** Enable max compatibility support with RFC2616 (HTTP) standard.
    Allows to support more features in the HTTP server.
    This increases the binary size
    Default: 0 */
#define MaxSupport            CONFIG_ESP_EHTTPD_MAX_SUPPORT

#endif
