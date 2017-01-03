#include "UdpSocket.h"

#include <cassert>

#if defined(WIN32)
    #pragma comment(lib, "ws2_32.lib")
#endif

using namespace CEC;

AddressInfo CEC::GetAddressInfo(std::string const& host)
{
    addrinfo* ai;
    if (getaddrinfo(host.c_str(), nullptr, nullptr, &ai) != 0)
        return{ nullptr, [](auto) {} };

    return AddressInfo{ ai, &::freeaddrinfo };
}

Private::SocketHandle UdpClientSocket::makeSocket() const
{
    return Private::SocketHandle{ socket(AF_INET, SOCK_DGRAM, 0) };
}

UdpClientSocket::UdpClientSocket(char const* connectionString)
    : name_(connectionString)
    , socket_()
{
    if (strncmp("udp:", connectionString, 4) != 0)
        return;

    connectionString += 4;

    auto hostBegin = connectionString;
    for (; *connectionString != ',' && *connectionString != '\x00'; ++connectionString)
    { }

    if (*connectionString != ',')
        return;

    auto const host = std::string{ hostBegin, connectionString++ };
    auto const port = std::strtoul(connectionString, const_cast<char**>(&connectionString), 10);
    if (port > std::numeric_limits<uint16_t>::max())
        return;

    if (*connectionString != ',')
        return;

    host_ = host;
    port_ = static_cast<uint16_t>(port);
}

UdpClientSocket::~UdpClientSocket()
{
    // Is it expected that the socket is closed or should destructor take care of that?
    UdpClientSocket::Close();
}

bool UdpClientSocket::Open(uint64_t iTimeoutMs)
{
    // preconditions
    if (socket_)
        return false;
    
    socket_ = OpenClient();
    return static_cast<bool>(socket_);
}

Private::SocketHandle UdpClientSocket::OpenClient() const
{
    if (auto ai = GetAddressInfo(host_))
    {
        // If the socket sockfd is of type SOCK_DGRAM then addr is the address to which datagrams
        // are sent by default, and the only address from which datagrams are received.

        if (auto s = makeSocket())
        {
            if (::connect(s.get(), ai->ai_addr, ai->ai_addrlen) != SOCKET_ERROR)
                return s;
        }
    }

    return{};
}

void UdpClientSocket::Close()
{
    socket_.reset();
}

void UdpClientSocket::Shutdown()
{
    // ?
}

bool UdpClientSocket::IsOpen()
{
    return static_cast<bool>(socket_);
}

ssize_t UdpClientSocket::Write(void* data, size_t len)
{
    return send(socket_.get(), static_cast<char const*>(data), static_cast<int>(len), 0);
}

ssize_t UdpClientSocket::Read(void* data, size_t len, uint64_t iTimeoutMs)
{
    return recv(socket_.get(), static_cast<char*>(data), static_cast<int>(len), 0);
}

std::string UdpClientSocket::GetError()
{
    return "";
}

int UdpClientSocket::GetErrorNumber()
{
    return 0;
}
