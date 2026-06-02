#pragma once
#include <winsock2.h>
#include <cstdint>

// Sends LXEM frames (16-byte header + channel-major float payload) over UDP.
// One socket is opened in Init() and reused for every Send() (no per-frame alloc
// of the socket). Header layout matches src/polyg_lsl/protocol.py exactly.
class Forwarder {
public:
    Forwarder();
    ~Forwarder();
    bool Init(const char* host, unsigned short port);
    bool Send(uint32_t seq, const float* data,
              uint16_t num_channels, uint16_t samples_per_channel);
    void Close();
private:
    SOCKET m_sock;
    sockaddr_in m_dest;
    bool m_wsa;
};
