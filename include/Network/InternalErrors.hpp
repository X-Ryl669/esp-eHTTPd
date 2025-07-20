#ifndef hpp_InternalErrors_hpp
#define hpp_InternalErrors_hpp

// We need types
#include "Types.hpp"

namespace Network
{
    /** The list of potential errors from the interface. Higher level than system call's error, hopefully will give a more meaningful name to errors and debugging */
    enum Errors
    {
        Success = 0,                //!< No error, operation succeeded
        SocketCreation,             //!< Socket creation error
        SocketOption,               //!< Error while changing socket options
        Bind,                       //!< Error while binding the socket
        Listen,                     //!< Error while listening the socket
        Select,                     //!< Error while selecting the socket
        Accept,                     //!< Error while accepting client
        Receiving,                  //!< Error while receiving
        Sending,                    //!< Error while sending
        ArgumentsMissing,           //!< Arguments are missing or bad
        BadCertificate,             //!< The given certificate is bad
        BadPrivateKey,              //!< The given private key is bad
        SSLConfig,                  //!< SSL configuration failed
        SSLBootstrap,               //!< Bootstraping SSL library failed
        SSLRandom,                  //!< SSL random number generator failed
        SSLSetup,                   //!< Error while setting up SSL
        SSLHandshake,               //!< SSL handshaking error
        BadSocketType,              //!< Bad socket type
        Timeout,                    //!< The operation timed out
        AllocationFailure,          //!< An allocation failed or misbehaved
    };

    /** An error type that dealing with usual POSIX calling convention of using 0 for success and negative value for error, positive for count */
    struct Error
    {
        Errors error;
        Error(int ret) : error((Errors)-ret) {}
        Error(Errors e) : error(e) {}

        bool isError() const { return (int)error > 0; }
        int getCount() const { return error <= 0 ? (int)-error : 0; }
        bool operator ==(const std::size_t count) const { return (std::size_t)getCount() == count; }
        bool operator ==(const int count) const { return getCount() == count; }
        bool operator ==(const Errors e) const { return error == e; }
        operator Errors() const { return error; }
    };

    /** The error or log level */
    enum class Level
    {
        Debug   = 0,
        Info    = 1,
        Warning = 2,
        Error   = 3,
    };
}

#endif
