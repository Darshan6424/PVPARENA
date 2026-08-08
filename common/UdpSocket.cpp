#include "UdpSocket.h"
#include <cstring>
#include <cstdio>

#ifdef _WIN32
    #pragma comment(lib, "ws2_32.lib")
    int UdpSocket::winsockRefCount_ = 0;

    bool UdpSocket::ensureWinsock() {
        if (winsockRefCount_++ == 0) {
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                winsockRefCount_--;
                return false;
            }
        }
        return true;
    }
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
#endif

UdpSocket::UdpSocket() {
#ifdef _WIN32
    ensureWinsock();
#endif
    sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
    open_ = (sock_ != INVALID_SOCKET);
#else
    open_ = (sock_ >= 0);
#endif
}

UdpSocket::~UdpSocket() {
    close();
#ifdef _WIN32
    if (--winsockRefCount_ == 0) {
        WSACleanup();
    }
#endif
}

bool UdpSocket::bind(uint16_t port) {
    if (!open_) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    int result = ::bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (result != 0) {
        std::fprintf(stderr, "UdpSocket::bind failed on port %u\n", port);
        return false;
    }

    // Make the socket non-blocking so the game loop never stalls waiting
    // on the network.
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock_, FIONBIO, &mode);
#else
    int flags = fcntl(sock_, F_GETFL, 0);
    fcntl(sock_, F_SETFL, flags | O_NONBLOCK);
#endif
    return true;
}

void UdpSocket::close() {
    if (!open_) return;
#ifdef _WIN32
    closesocket(sock_);
#else
    ::close(sock_);
#endif
    open_ = false;
}

bool UdpSocket::send(const void* data, size_t size, const Endpoint& to) {
    if (!open_) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(to.port);
    if (inet_pton(AF_INET, to.ip.c_str(), &addr.sin_addr) != 1) {
        return false;
    }

    int sent = ::sendto(sock_, reinterpret_cast<const char*>(data),
                         static_cast<int>(size), 0,
                         reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    return sent == static_cast<int>(size);
}

int UdpSocket::receive(void* buffer, size_t bufferSize, Endpoint& from) {
    if (!open_) return -1;

    sockaddr_in addr{};
#ifdef _WIN32
    int addrLen = sizeof(addr);
#else
    socklen_t addrLen = sizeof(addr);
#endif

    int received = ::recvfrom(sock_, reinterpret_cast<char*>(buffer),
                               static_cast<int>(bufferSize), 0,
                               reinterpret_cast<sockaddr*>(&addr), &addrLen);

    if (received < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) return 0;
#else
        if (errno == EWOULDBLOCK || errno == EAGAIN) return 0;
#endif
        return -1;
    }

    char ipBuf[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &addr.sin_addr, ipBuf, sizeof(ipBuf));
    from.ip = ipBuf;
    from.port = ntohs(addr.sin_port);

    return received;
}
