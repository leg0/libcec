#include "UdpSocket.h"

#include <cassert>

#if defined(WIN32)
    #pragma comment(lib, "ws2_32.lib")
#endif

using namespace CEC;

UdpSocket::AddrInfoPtr UdpSocket::MyGetAddrInfo(std::string const& host)
{
    addrinfo* ai;
    if (getaddrinfo(host.c_str(), nullptr, nullptr, &ai) != 0)
        return{};

    return AddrInfoPtr{ ai };
}

Private::SocketHolder UdpSocket::makeSocket() const
{
    //auto const af = ipv_ == IpVersion::Ipv4 ? AF_INET : AF_INET6;
    return Private::SocketHolder{ socket(AF_INET, SOCK_DGRAM, 0) };
}

UdpSocket::UdpSocket(char const* connectionString)
    : name_(connectionString)
    , socket_(INVALID_SOCKET)
    , isServer_(false)
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

    sockaddr_in sa = { 0 };
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port_);
    sa.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    mostRecentPeerAddress_ = sockaddr_storage{ 0 };
    memcpy(&mostRecentPeerAddress_, &sa, sizeof(sa));
}

UdpSocket::~UdpSocket()
{
    // Is it expected that the socket is closed or should destructor take care of that?
    UdpSocket::Close();
}

bool UdpSocket::Open(uint64_t iTimeoutMs)
{
    // preconditions
    if (socket_ != INVALID_SOCKET)
        return false;
    
    if (isServer_)
        return OpenServer();
    else
        return OpenClient();
}

bool UdpSocket::OpenServer()
{
    sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (auto s = makeSocket())
    {
        if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != SOCKET_ERROR)
        {
            socket_ = std::move(s);
            return true;
        }
    }

    return false;
}

bool UdpSocket::OpenClient()
{
    if (AddrInfoPtr ai = MyGetAddrInfo(host_))
    {
        // If the socket sockfd is of type SOCK_DGRAM then addr is the address to which datagrams
        // are sent by default, and the only address from which datagrams are received.

        if (auto s = makeSocket())
        {
            if (::connect(s, ai->ai_addr, ai->ai_addrlen) != SOCKET_ERROR)
            {

                socket_ = std::move(s);
                //to_ = std::move(ai);
                return true;
            }
        }
    }

    return false;
}

void UdpSocket::Close()
{
    //to_.reset();
    socket_.reset();
}

void UdpSocket::Shutdown()
{
    // ?
}

bool UdpSocket::IsOpen()
{
    return socket_;
}

ssize_t UdpSocket::Write(void* data, size_t len)
{
    if (isServer_)
    {
    }
    else
    {
        return send(socket_, static_cast<char const*>(data), static_cast<int>(len), 0);
    }
}

ssize_t UdpSocket::Read(void* data, size_t len, uint64_t iTimeoutMs)
{
    if (isServer_)
    {
        sockaddr_storage from;
        int fromSize = sizeof(from);
        auto const res = recvfrom(socket_, static_cast<char*>(data), static_cast<int>(len), 0, reinterpret_cast<sockaddr*>(&from), &fromSize);
        if (res >= 0)
            mostRecentPeerAddress_ = from;
        return res;
    }
    else
    {
        return recv(socket_, static_cast<char*>(data), static_cast<int>(len), 0);
    }
}

std::string UdpSocket::GetError()
{
    return "";
}

int UdpSocket::GetErrorNumber()
{
    return 0;
}
