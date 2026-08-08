#pragma once
// UdpSocket.h
// Minimal raw-UDP wrapper. No framework - just BSD sockets on Linux/macOS
// and Winsock on Windows behind the same small interface.

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

    bool operator==(const Endpoint& o) const {
        return ip == o.ip && port == o.port;
    }
    bool operator!=(const Endpoint& o) const { return !(*this == o); }
};

class UdpSocket {
public:
    UdpSocket();
    ~UdpSocket();

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    // Bind to a local port. Pass 0 to let the OS pick an ephemeral port
    // (what a client does); pass a fixed port for the server.
    bool bind(uint16_t port);

    void close();

    // Fire-and-forget send. Returns false on hard failure.
    bool send(const void* data, size_t size, const Endpoint& to);

    // Non-blocking receive. Returns number of bytes received (0 if
    // nothing was waiting, -1 on error). Fills `from` with the sender.
    int receive(void* buffer, size_t bufferSize, Endpoint& from);

    bool isOpen() const { return open_; }

private:
    socket_t sock_{};
    bool     open_ = false;
#ifdef _WIN32
    static int  winsockRefCount_;
    static bool ensureWinsock();
#endif
};
