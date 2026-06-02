#include "Forwarder.h"
#include <ws2tcpip.h>
#include <cstring>
#include <vector>
#pragma comment(lib, "Ws2_32.lib")

static const uint32_t LXEM_MAGIC = 0x4C58454D; // 'LXEM'
static const uint16_t LXEM_VERSION = 1;

Forwarder::Forwarder() : m_sock(INVALID_SOCKET), m_wsa(false) {
    std::memset(&m_dest, 0, sizeof(m_dest));
}

Forwarder::~Forwarder() { Close(); }

bool Forwarder::Init(const char* host, unsigned short port) {
    WSADATA w;
    if (WSAStartup(MAKEWORD(2, 2), &w) != 0) return false;
    m_wsa = true;
    m_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_sock == INVALID_SOCKET) return false;
    m_dest.sin_family = AF_INET;
    m_dest.sin_port = htons(port);
    inet_pton(AF_INET, host, &m_dest.sin_addr);
    return true;
}

bool Forwarder::Send(uint32_t seq, const float* data,
                     uint16_t nch, uint16_t spc) {
    if (m_sock == INVALID_SOCKET) return false;
    const size_t payload = static_cast<size_t>(nch) * spc * sizeof(float);
    std::vector<char> buf(16 + payload);
    char* p = buf.data();
    std::memcpy(p + 0, &LXEM_MAGIC, 4);
    std::memcpy(p + 4, &LXEM_VERSION, 2);
    std::memcpy(p + 6, &nch, 2);
    std::memcpy(p + 8, &spc, 2);
    uint16_t flags = 0;
    std::memcpy(p + 10, &flags, 2);
    std::memcpy(p + 12, &seq, 4);
    std::memcpy(p + 16, data, payload);
    int sent = sendto(m_sock, buf.data(), static_cast<int>(buf.size()), 0,
                      reinterpret_cast<sockaddr*>(&m_dest), sizeof(m_dest));
    return sent == static_cast<int>(buf.size());
}

void Forwarder::Close() {
    if (m_sock != INVALID_SOCKET) { closesocket(m_sock); m_sock = INVALID_SOCKET; }
    if (m_wsa) { WSACleanup(); m_wsa = false; }
}
