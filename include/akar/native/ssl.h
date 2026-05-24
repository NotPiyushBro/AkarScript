#pragma once
// Akar Script - SSL/TLS native support
// Uses openssl s_client subprocess (no external C library dependency)

#include <string>

namespace akar {

class VM;
struct ObjMap;

// Low-level SSL handle
struct SslHandle {
    int pid = -1;
    int write_fd = -1;
    int read_fd = -1;
    bool valid() const { return pid > 0 && write_fd >= 0 && read_fd >= 0; }
};

// Raw C++ API (used by http.cpp)
bool ssl_available();
SslHandle ssl_connect_raw(const std::string& host, int port);
int ssl_send_raw(const SslHandle& h, const std::string& data);
std::string ssl_recv_raw(const SslHandle& h, int max_bytes = 4096, int timeout_ms = 10000);
std::string ssl_recv_until(const SslHandle& h, const std::string& delim, int timeout_ms = 10000, int max_bytes = 65536);
std::string ssl_recv_all(const SslHandle& h, int timeout_ms = 10000, int max_bytes = 65536);
void ssl_close_raw(SslHandle& h);

// Register SSL natives (adds to existing net module)
void register_ssl_native(VM& vm, ObjMap* net_mod);

} // namespace akar
