#ifndef hpp_Socket_hpp
#define hpp_Socket_hpp

// We need our configuration
#include "HTTPDConfig.hpp"

// We need basic errors
#include "InternalErrors.hpp"


// We need BSD socket here
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>
// We need TCP_NODELAY
#include <netinet/tcp.h>
// We need sockaddr_in
#include <netinet/in.h>
#if UseTLS == 1
// We need MBedTLS code
#include <mbedtls/certs.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/platform.h>
#include <mbedtls/ssl.h>
#endif

#if UseTLS == 1
    // Small optimization to remove useless virtual table in the final binary if not used
    #define Virtual virtual
#else
    #define Virtual
#endif

namespace Network {

    #define IPV4StrAddressLen   sizeof("255.255.255.255:65535")

    #define closesocket         close

    static struct timeval timeoutFromMs(const uint32 timeout)
    {
        return timeval { (time_t)(timeout / 1024), // Avoid division here (compiler should shift the value here), the value is approximative anyway
                         (suseconds_t)((timeout & 1023) * 977)};  // Avoid modulo here and make sure it doesn't overflow (since 1023 * 977 < 1000000)
    }

    /** The base socket that's used in the server, using plain old IPv4 and no specific code */
    struct BaseSocket
    {
        int                      socket;
        char                     address[IPV4StrAddressLen];

        /** Start listening on the socket
            @return 0 on success, negative value upon error */
        Virtual Error listen(uint16 port, int maxClientCount = 1)
        {
            socket = ::socket(AF_INET, SOCK_STREAM, 0);
            if (socket == -1) return SocketCreation;

            // Make sure we can bind on an already bound address
            int n = 1;
            if (::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (const char *) &n, sizeof(n)) != 0) return SocketOption;

            struct sockaddr_in address;
            address.sin_port = htons(port);
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_ANY);

            if (int ret = ::bind(socket, (const sockaddr*)&address, sizeof(address)); ret < 0) return Bind;
            if (int ret = ::listen(socket, maxClientCount); ret < 0) return Listen;

            return Success;
        }


        /** Accept a new client.
            @return 0 on success, negative value upon error */
        Virtual Error accept(BaseSocket & clientSocket, const uint32 timeoutMillis = 0)
        {
            // Check for activity on the socket
            if (timeoutMillis) {
                if (Error ret = select(true, false, timeoutMillis); ret.isError()) return ret;
            }

            struct sockaddr_in clientAddress = {};
            socklen_t addrLen = sizeof(clientAddress);
            int ret = ::accept(socket, (sockaddr*)&clientAddress, &addrLen);
            if (ret == -1) return Accept;

            clientSocket.socket = ret;
            sprintf(clientSocket.address, "%u.%u.%u.%u:%u", (unsigned)((clientAddress.sin_addr.s_addr >> 0) & 0xFF), (unsigned)((clientAddress.sin_addr.s_addr >> 8) & 0xFF), (unsigned)((clientAddress.sin_addr.s_addr >> 16) & 0xFF), (unsigned)((clientAddress.sin_addr.s_addr >> 24) & 0xFF), (unsigned)clientAddress.sin_port);
            return Success;
        }

        Virtual Error recv(char * buffer, const uint32 maxLength = 0, const uint32 minLength = 0)
        {
            int ret = 0;
            if (minLength) {
                ret = ::recv(socket, buffer, minLength, MSG_WAITALL);
                if (ret <= 0) return ret;
                if (maxLength <= minLength) return ret;
            }
            int nret = ::recv(socket, &buffer[ret], maxLength - ret, 0);
            return nret <= 0 ? nret : nret + ret;
        }

        Virtual Error send(const char * buffer, const uint32 length)
        {
            return ::send(socket, buffer, (int)length, 0);
        }

        // Useful socket helpers functions here
        Virtual Error select(bool reading, bool writing, const uint32 timeoutMillis = (uint32)-1)
        {
            // Linux modifies the timeout when calling select
            struct timeval v = timeoutFromMs(timeoutMillis);

            fd_set set;
            FD_ZERO(&set);
            FD_SET(socket, &set);
            // Then select
            int ret = ::select(socket + 1, reading ? &set : NULL, writing ? &set : NULL, NULL, timeoutMillis == (uint32)-1 ? NULL : &v);
            if (ret < 0) return Select;
            if (ret == 0) return Timeout;
            return ret;
        }

        /** This is only used with SSL socket to avoid RTTI */
        Virtual int getType() const { return 0; }

        Virtual void reset() { ::closesocket(socket); socket = -1; }

        bool isValid() const { return socket != -1; }

        BaseSocket() : socket(-1) {}
        Virtual ~BaseSocket() { ::closesocket(socket); socket = -1; }
    };

#undef Virtual

#if UseTLS == 1
    class MBTLSSocket : public BaseSocket
    {
        mbedtls_entropy_context entropy;
        mbedtls_ctr_drbg_context entropySource;
        mbedtls_ssl_context ssl;
        mbedtls_ssl_config conf;
        mbedtls_x509_crt cacert;
        mbedtls_pk_context pk;
        mbedtls_net_context net;

    private:
        Error buildServerConf(const ROString & serverCert, const ROString & keyFile)
        {
            if (!serverCert || !keyFile) return ArgumentsMissing;

            // Use given root certificate (if you have a recent version of mbedtls, you could use mbedtls_x509_crt_parse_der_nocopy instead to skip a useless copy here)
            if (::mbedtls_x509_crt_parse_der(&cacert, serverCert.getData(), serverCert.getLength()))
                return BadCertificate;

            if (::mbedtls_pk_parse_key(&pk, keyFile.getData(), keyFile.getLength(), NULL, 0))
                return BadPrivateKey;

            // Now create configuration from default
            if (::mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT))
                return SSLConfig;

            return buildConf(true, true);
        }

        Error buildClientConf(const ROString & serverCert)
        {

            // Use given root certificate (if you have a recent version of mbedtls, you could use mbedtls_x509_crt_parse_der_nocopy instead to skip a useless copy here)
            if (serverCert && ::mbedtls_x509_crt_parse_der(&cacert, serverCert.getData(), serverCert.getLength()))
                return BadCertificate;

            // Now create configuration from default
            if (::mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT))
                return SSLConfig;

            return buildConf(serverCert, false);
        }

        Error buildConf(bool hasCert, bool hasPKey)
        {
            ::mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL); // Example use cacert.next, check this
            if (hasPKey && ::mbedtls_ssl_conf_own_cert(&conf, &cacert, &pk)) return BadCertificate;
            else ::mbedtls_ssl_conf_authmode(&conf, hasCert ? MBEDTLS_SSL_VERIFY_REQUIRED : MBEDTLS_SSL_VERIFY_NONE);

            uint32_t ms = timeoutMs.tv_usec / 1000;
            ::mbedtls_ssl_conf_read_timeout(&conf, ms < 50 ? 3000 : ms);

            // Random number generator
            ::mbedtls_ssl_conf_rng(&conf, ::mbedtls_ctr_drbg_random, &entropySource);
            if (::mbedtls_ctr_drbg_seed(&entropySource, ::mbedtls_entropy_func, &entropy, NULL, 0))
                return SSLRandom;

            if (::mbedtls_ssl_setup(&ssl, &conf))
                return SSLSetup;

            return Success;
        }

    public:
        MBTLSSocket(struct timeval & timeoutMs) : BaseSocket(timeoutMs)
        {
            mbedtls_ssl_init(&ssl);
            mbedtls_ssl_config_init(&conf);
            mbedtls_x509_crt_init(&cacert);
            mbedtls_ctr_drbg_init(&entropySource);
            mbedtls_entropy_init(&entropy);
            mbedtls_net_init(&net);
            mbedtls_pk_init(&pk);
        }

        Error listen(uint16 port, int maxClientCount = 1)
        {
            Error ret = BaseSocket::listen(port, maxClientCount);
            if (ret.isError()) return ret;

            net.fd = socket;

            mbedtls_ssl_session_reset(&ssl);
            return Success;
        }
/*
        int connect(const char * host, uint16 port, const v5::DynamicBinDataView * brokerCert)
        {
            int ret = BaseSocket::connect(host, port, 0);
            if (ret) return ret;

            // MBedTLS doesn't deal with natural socket timeout correctly, so let's fix that
            struct timeval zeroTO = {};
            if (::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &zeroTO, sizeof(zeroTO)) < 0) return -4;
            if (::setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &zeroTO, sizeof(zeroTO)) < 0) return -4;

            net.fd = socket;

            if (!buildConf(brokerCert))                                             return -8;
            if (::mbedtls_ssl_set_hostname(&ssl, host))                             return -9;

            // Set the method the SSL engine is using to fetch/send data to the other side
            ::mbedtls_ssl_set_bio(&ssl, &net, ::mbedtls_net_send, NULL, ::mbedtls_net_recv_timeout);

            ret = ::mbedtls_ssl_handshake(&ssl);
            if (ret != 0 && ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
                return -10;

            // Check certificate if one provided
            if (brokerCert)
            {
                uint32_t flags = mbedtls_ssl_get_verify_result(&ssl);
                if (flags != 0)
                {
                    char verify_buf[100] = {0};
                    mbedtls_x509_crt_verify_info(verify_buf, sizeof(verify_buf), "  ! ", flags);
                    printf("mbedtls_ssl_get_verify_result: %s flag: 0x%x\n", verify_buf, (unsigned int)flags);
                    return -11;
                }
            }
            return 0;
        }
*/
        /** Accept a new client.
            @return 0 on success, negative value upon error */
        Error accept(BaseSocket & clientSocket, const uint32 timeoutMillis = 0)
        {
            if (clientSocket.getType() != 1) return BadSocketType;
            // Check for activity on the socket
            if (timeoutMillis) {
                if (Error ret = select(true, false, timeoutMillis); ret.isError()) return ret;
            }

            MBTLSSocket & client = (MBTLSSocket&)clientSocket;
            client.reset();

            // Fetch the client address too
            struct sockaddr_in clientAddress = {};
            socklen_t addrLen = sizeof(clientAddress);
            size_t clientAddrLen = 0;
            int ret = mbedtls_net_accept(&net, &client.net, &clientAddress, addrLen, &clientAddrLen);
            if (ret != 0) return Accept;

            sprintf(clientSocket.address, "%u.%u.%u.%u:%u", (unsigned)((clientAddress.sin_addr.s_addr >> 0) & 0xFF), (unsigned)((clientAddress.sin_addr.s_addr >> 8) & 0xFF), (unsigned)((clientAddress.sin_addr.s_addr >> 16) & 0xFF), (unsigned)((clientAddress.sin_addr.s_addr >> 24) & 0xFF), (unsigned)clientAddress.sin_port);
            clientSocket.socket = client.net.fd; // Also save the file descriptor here

            // Perform handshake now
            mbedtls_ssl_set_bio(&ssl, &client.net, mbedtls_net_send, mbedtls_net_recv, NULL);
            while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
                // Check the error code for any possible issue with handshaking
                if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                    return SSLHandshake;
                }
            }

            return Success;
        }

        Error send(const char * buffer, const uint32 length)
        {
            return ::mbedtls_ssl_write(&ssl, (const uint8*)buffer, length);
        }

        Error recv(char * buffer, const uint32 minLength, const uint32 maxLength = 0)
        {
            uint32 ret = 0;
            while (ret < minLength)
            {
                int r = ::mbedtls_ssl_read(&ssl, (uint8*)&buffer[ret], minLength - ret);
                if (r <= 0)
                {
                    // Those means that we need to call again the read method
                    if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE)
                        continue;
                    if (r == MBEDTLS_ERR_SSL_TIMEOUT) {
                        return Timeout;
                    }
                    return ret ? (int)ret : r; // Silent error here
                }
                ret += (uint32)r;
            }
            if (maxLength <= minLength) return ret;

            // This one is a non blocking call
            int nret = ::mbedtls_ssl_read(&ssl, (uint8*)&buffer[ret], maxLength - ret);
            if (nret == MBEDTLS_ERR_SSL_TIMEOUT) return Timeout;
            return nret <= 0 ? nret : nret + ret;
        }

        ~MBTLSSocket()
        {
            mbedtls_ssl_close_notify(&ssl);
            mbedtls_x509_crt_free(&cacert);
            mbedtls_entropy_free(&entropy);
            mbedtls_ssl_config_free(&conf);
            mbedtls_ctr_drbg_free(&entropySource);
            mbedtls_pk_free(&pk);
            mbedtls_ssl_free(&ssl);
        }

        /** This is only used with SSL socket to avoid RTTI */
        int getType() const { return 1; }

        void reset() { mbedtls_net_free(&net); }
    };
#endif

    /** A socket pool used to select multiple socket at once.
        The order of the sockets in the pool isn't preserved upon removing sockets (removing is done with swapping with the last used element in the array).
        Appending sockets are always done to the end of the pool.  */
    template <std::size_t N>
    struct SocketPool
    {
        BaseSocket *    sockets[N] = {};
        std::size_t     used = 0;
        uint32          selectMask;

        /** Append a socket to the pool */
        bool append(BaseSocket & socket) {
            if (used == N) return false;
            sockets[used++] = &socket;
            return true;
        }
        /** Remove a socket from the pool */
        bool remove(BaseSocket & socket) {
            if (!used) return false;
            // Find the socket to remove
            for (std::size_t i = 0; i < N; i++)
            {
                if (sockets[i] == &socket) {
                    std::size_t u = used - 1;
                    sockets[i] = sockets[u]; // Swap with last
                    if (selectMask & (1<<u))
                        // Swap the select status too
                        selectMask ^= (1<<u) | ((selectMask & (1<<i)) ? 0 : (1<<i));
                    sockets[u] = 0;
                    --used;
                    return true;
                }
            }
            return false;
        }
        /** Select the sockets that are active for reading. Use this and getReadableSocket() to fetch the socket that's readable
            @return positive value upon any socket readable in the pool, 0 for timeout, negative value upon error */
        Error selectActive(const uint32 timeoutMillis = (uint32)-1)
        {
            // Linux modifies the timeout when calling select
            struct timeval v = timeoutFromMs(timeoutMillis);
            selectMask = 0;

            fd_set set;
            int max = 0;
            FD_ZERO(&set);
            for (std::size_t i = 0; i < used; i++) {
                if (sockets[i] == 0) return -1; // Impossible case, should log it
                FD_SET(sockets[i]->socket, &set);
                max = max > sockets[i]->socket ? max : sockets[i]->socket;
            }
            // Then select
            int ret = ::select(max + 1, &set, NULL, NULL, timeoutMillis == (uint32)-1 ? NULL : &v);
            if (ret == 0) return Timeout;
            if (ret < 0) return ret;
            for (std::size_t i = 0; i < used; i++) {
                if (FD_ISSET(sockets[i]->socket, &set)) selectMask |= (uint32)(1<<i);
            }
            return Success;
        }

        /** Get the next readable socket. This doesn't work without having called selectActive() first (and it returned > 0)
            @return 0 if no more readable socket is available or the socket's pointer else */
        BaseSocket * getReadableSocket(std::size_t startPos = 0)
        {
            if (!selectMask) return 0;
            for (std::size_t i = startPos; i < used; i++) {
                if ((selectMask & (1<<i))) {
                    selectMask ^= (uint32)(1<<i);
                    return sockets[i];
                }
            }
            // Should never happens
            return 0;
        }
        /** Check if a specific socket position is readable */
        bool isReadable(std::size_t pos) const { return selectMask & (1<<pos); }

        SocketPool() : used(0), selectMask(0) { Zero(sockets); }
    };


}

#endif