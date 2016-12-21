#pragma once

#include "env.h"
#include "p8-platform/sockets/socket.h"

#include <cstdint>
#include <memory>
#include <string>

#if !defined(HAVE_UDP_TRANSPORT)
#error "You must have HAVE_UDP_TRANSPORT defined in order to include this header"
#endif

namespace CEC {

    namespace Private {
        struct SocketHolder
        {
            SocketHolder() : s(INVALID_SOCKET) { }
            explicit SocketHolder(SOCKET s) : s(s) { }
            SocketHolder(SocketHolder const&) = delete;
            SocketHolder(SocketHolder&& src)
                : s(src.s)
            {
                src.s = INVALID_SOCKET;
            }

            SocketHolder& operator=(SocketHolder const&) = delete;
            SocketHolder& operator=(SocketHolder&& src)
            {
                if (this != &src)
                {
                    reset(src.s);
                    src.s = INVALID_SOCKET;
                }
                return *this;
            }

            ~SocketHolder()
            {
                reset();
            }

            SOCKET reset(SOCKET newsck = INVALID_SOCKET)
            {
                SOCKET oldsck = s;
                if (newsck != s)
                {
                    ::closesocket(s);
                    s = newsck;
                }
                return oldsck;
            }

            operator SOCKET() const { return s; }
            explicit operator bool() const
            {
                return s != INVALID_SOCKET;
            }

        private:
            SOCKET s;
        };

        class WinsockHelper
        {
            WinsockHelper(WinsockHelper const&) = delete;
            WinsockHelper& operator=(WinsockHelper const&) = delete;
            WinsockHelper(WinsockHelper&&) = delete;
            WinsockHelper& operator=(WinsockHelper&&) = delete;
#if defined(WIN32)
            WinsockHelper()
                : success_(::WSAStartup(MAKEWORD(2, 2), nullptr) == 0)
            { }

            ~WinsockHelper()
            {
                if (success_)
                    ::WSACleanup();
            }
        private:
            bool const success_;
#else
            WinsockHelper() = default;
#endif
        };
    }

    class UdpSocket : public P8PLATFORM::ISocket, Private::WinsockHelper
    {
        struct AddrInfoDeleter
        {
            void operator()(addrinfo* p) const { ::freeaddrinfo(p); }
        };
        using AddrInfoPtr = std::unique_ptr<addrinfo, AddrInfoDeleter>;

    public:
        // udp:[a-z0-9.-]+,[0-9]+
        explicit UdpSocket(char const* strPort);
        ~UdpSocket();

        bool Open(uint64_t iTimeoutMs = 0) override;
        void Close() override;
        void Shutdown() override;
        bool IsOpen() override;
        ssize_t Write(void* data, size_t len) override;
        ssize_t Read(void* data, size_t len, uint64_t iTimeoutMs = 0) override;
        std::string GetError() override;
        int GetErrorNumber() override;
        std::string GetName() override { return name_; }

    private:

        static AddrInfoPtr UdpSocket::MyGetAddrInfo(std::string const& host);
        Private::SocketHolder makeSocket() const;
        bool OpenServer();
        bool OpenClient();

        std::string const name_;
        std::string host_;
        uint16_t port_;
        Private::SocketHolder socket_;
        sockaddr_storage mostRecentPeerAddress_;
        bool isServer_;
    };

    class ProtectedUdpSocket : public P8PLATFORM::CProtectedSocket<UdpSocket>
    {
    public:
        explicit ProtectedUdpSocket(char const* strPort)
            : P8PLATFORM::CProtectedSocket<UdpSocket>(new UdpSocket(strPort))
        { }

        ~ProtectedUdpSocket() override { }
    };
}