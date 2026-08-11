#pragma once
// Thin wrapper over BSD sockets / Winsock so the rest of the code doesn't
// have to care which one it got.

#include <cstdint>
#include <cstddef>
#include <string>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using socket_t = SOCKET;
#else
    using socket_t = int;
#endif

struct Endpoint {
    std::string ip;
    uint16_t    port = 0;

    bool operator==(const Endpoint& o) const { return ip == o.ip && port == o.port; }
    bool operator!=(const Endpoint& o) const { return !(*this == o); }
};

class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    // Port 0 lets the OS pick an ephemeral one, which is what clients want.
    // Reopens the underlying socket if it was closed, so a socket can be
    // rebound after close() instead of being dead for good.
    bool bind(uint16_t port);

    void close();

    bool send(const void* data, size_t size, const Endpoint& to);

    // Non-blocking. Returns bytes read, 0 if nothing was waiting, -1 on error.
    int receive(void* buffer, size_t bufferSize, Endpoint& from);

    bool isOpen() const { return open_; }

    // The LAN address other machines would use to reach this one, so the host
    // doesn't have to go dig it out of ipconfig. "127.0.0.1" if we can't tell.
    static std::string getLocalIPAddress();

private:
    bool openSocket();

    socket_t sock_{};
    bool     open_ = false;
#ifdef _WIN32
    static int  winsockRefCount_;
    static bool ensureWinsock();
#endif
};
