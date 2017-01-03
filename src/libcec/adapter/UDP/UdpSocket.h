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
        struct SocketHandle
        {
            SocketHandle() : s(INVALID_SOCKET) { }
            explicit SocketHandle(SOCKET s) : s(s) { }
            SocketHandle(SocketHandle const&) = delete;
            SocketHandle(SocketHandle&& src)
                : s(src.s)
            {
                src.s = INVALID_SOCKET;
            }

            SocketHandle& operator=(SocketHandle const&) = delete;
            SocketHandle& operator=(SocketHandle&& src)
            {
                if (this != &src)
                {
                    reset(src.s);
                    src.s = INVALID_SOCKET;
                }
                return *this;
            }

            ~SocketHandle()
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

            SOCKET get() const { return s; }
            explicit operator bool() const
            {
                return s != INVALID_SOCKET;
            }

        private:
            SOCKET s;
        };

        class WinsockHelper
        {
        public:
            WinsockHelper(WinsockHelper const&) = delete;
            WinsockHelper(WinsockHelper&&) = delete;

            WinsockHelper& operator=(WinsockHelper const&) = delete;
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

    using AddressInfo = std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)>;
    AddressInfo GetAddressInfo(std::string const& host);

    class UdpClientSocket : public P8PLATFORM::ISocket, Private::WinsockHelper
    {

    public:
        UdpClientSocket() = default;

        // udp:[a-z0-9.-]+,[0-9]+
        explicit UdpClientSocket(char const* strPort);
        UdpClientSocket(UdpClientSocket const&) = delete;
        UdpClientSocket(UdpClientSocket&& src)
        {
            *this = std::move(src);
        }

        ~UdpClientSocket();

        UdpClientSocket& operator=(UdpClientSocket const&) = delete;
        UdpClientSocket& operator=(UdpClientSocket&& src)
        {
            if (this != &src)
            {
                name_ = std::move(src.name_);
                host_ = std::move(src.host_);
                port_ = src.port_;
                socket_ = std::move(src.socket_);
            }
            return *this;
        }

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

        Private::SocketHandle makeSocket() const;
        Private::SocketHandle OpenClient() const;

        std::string name_;
        std::string host_;
        uint16_t port_;
        Private::SocketHandle socket_;
    };

    class UdpServerSocket
    {

    };
    class ProtectedUdpSocket : public P8PLATFORM::CProtectedSocket<UdpClientSocket>
    {
    public:
        explicit ProtectedUdpSocket(char const* strPort)
            : P8PLATFORM::CProtectedSocket<UdpClientSocket>(new UdpClientSocket(strPort))
        { }

        ~ProtectedUdpSocket() override { }
    };
}