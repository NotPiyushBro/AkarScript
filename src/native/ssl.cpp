// Akar Script - SSL/TLS native support via openssl s_client subprocess
// No external C library dependency - uses fork+exec with openssl CLI

#include "akar/native/ssl.h"
#include "akar/vm/vm.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <climits>
#include <cstring>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace akar {

// ============================================================================
// Handle tracking (same pattern as existing g_open_sockets in system.cpp)
// ============================================================================
static std::unordered_map<int, SslHandle> g_ssl_handles;
static int g_next_ssl_handle = 1;
static ObjClass* g_ssl_cls = nullptr;
static bool g_openssl_checked = false;
static bool g_openssl_available = false;

// ============================================================================
// Helpers
// ============================================================================
static void mod_put(ObjMap* m, const char* name, NativeFn fn) {
    m->entries[name] = Value(static_cast<Obj*>(
        allocate_native([f = std::move(fn)](int argc, Value* argv) -> Value {
            if (argc >= 1) { argc--; argv++; }
            return f(argc, argv);
        }, name)));
}

static int safe_int(double v) {
    return (v >= INT_MIN && v <= INT_MAX) ? static_cast<int>(v) : 0;
}

static int get_handle(Value self) {
    if (!self.is_instance()) return -1;
    auto it = self.as_instance()->fields.find("_handle");
    if (it == self.as_instance()->fields.end()) return -1;
    if (!it->second.is_number()) return -1;
    return safe_int(it->second.get_number());
}

// ============================================================================
// Check if openssl CLI is available
// ============================================================================
bool ssl_available() {
    if (g_openssl_checked) return g_openssl_available;
    g_openssl_checked = true;
    g_openssl_available = (system("openssl version >/dev/null 2>&1") == 0);
    return g_openssl_available;
}

// ============================================================================
// SSL connect via fork+exec of openssl s_client
// ============================================================================
SslHandle ssl_connect_raw(const std::string& host, int port) {
    SslHandle h;
    if (!ssl_available()) return h;

    int to_child[2];   // parent writes, child reads (stdin)
    int from_child[2]; // child writes, parent reads (stdout)

    if (pipe(to_child) < 0) return h;
    if (pipe(from_child) < 0) {
        close(to_child[0]); close(to_child[1]);
        return h;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(to_child[0]); close(to_child[1]);
        close(from_child[0]); close(from_child[1]);
        return h;
    }

    if (pid == 0) {
        // Child process: become openssl s_client
        close(to_child[1]);   // close write end of stdin pipe
        close(from_child[0]); // close read end of stdout pipe
        dup2(to_child[0], STDIN_FILENO);
        dup2(from_child[1], STDOUT_FILENO);
        close(to_child[0]);
        close(from_child[1]);

        // Redirect stderr to /dev/null to suppress handshake info
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        std::string connect_arg = host + ":" + std::to_string(port);
        execlp("openssl", "openssl", "s_client",
               "-quiet",       // suppress handshake output
               "-servername", host.c_str(),  // SNI support
               "-connect", connect_arg.c_str(),
               (char*)nullptr);
        _exit(127); // exec failed
    }

    // Parent process
    close(to_child[0]);   // close read end of stdin pipe
    close(from_child[1]); // close write end of stdout pipe

    h.pid = pid;
    h.write_fd = to_child[1];
    h.read_fd = from_child[0];

    // Wait for TLS handshake (try reading with timeout)
    // openssl s_client with -quiet outputs nothing on success
    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(h.read_fd, &fds);
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    int ready = select(h.read_fd + 1, &fds, NULL, NULL, &tv);
    if (ready > 0) {
        // Some data available - could be handshake error output or server greeting
        char buf[1024];
        ssize_t n = read(h.read_fd, buf, sizeof(buf));
        if (n <= 0) {
            // Connection closed immediately = failure
            ssl_close_raw(h);
            return SslHandle{};
        }
        // Store any initial data (server greeting, etc.) for first recv
        h.pending_data.assign(buf, n);
    }
    // If ready == 0 (timeout), the handshake might still be in progress.
    // That's OK - we'll let the caller handle it.

    // Verify child is still running
    int status;
    pid_t result = waitpid(h.pid, &status, WNOHANG);
    if (result > 0) {
        // Child already exited = connection failed
        close(h.write_fd);
        close(h.read_fd);
        return SslHandle{};
    }

    return h;
}

// ============================================================================
// SSL I/O
// ============================================================================
int ssl_send_raw(const SslHandle& h, const std::string& data) {
    if (!h.valid()) return -1;
    ssize_t n = write(h.write_fd, data.c_str(), data.size());
    return static_cast<int>(n);
}

std::string ssl_recv_raw(SslHandle& h, int max_bytes, int timeout_ms) {
    if (!h.valid()) return "";

    // Return any pending data from handshake first
    if (!h.pending_data.empty()) {
        std::string result = h.pending_data.substr(0, max_bytes);
        h.pending_data.erase(0, result.size());
        return result;
    }

    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(h.read_fd, &fds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    if (select(h.read_fd + 1, &fds, NULL, NULL, &tv) <= 0) return "";

    std::vector<char> buf(max_bytes);
    ssize_t n = read(h.read_fd, buf.data(), max_bytes);
    if (n <= 0) return "";
    return std::string(buf.data(), n);
}

std::string ssl_recv_until(SslHandle& h, const std::string& delim,
                           int timeout_ms, int max_bytes) {
    if (!h.valid()) return "";
    std::string accumulated;
    while (static_cast<int>(accumulated.size()) < max_bytes) {
        std::string chunk = ssl_recv_raw(h, 4096, timeout_ms);
        if (chunk.empty()) break;
        accumulated += chunk;
        if (accumulated.find(delim) != std::string::npos) break;
    }
    return accumulated;
}

std::string ssl_recv_all(SslHandle& h, int timeout_ms, int max_bytes) {
    if (!h.valid()) return "";
    std::string accumulated;
    while (static_cast<int>(accumulated.size()) < max_bytes) {
        std::string chunk = ssl_recv_raw(h, 8192, timeout_ms);
        if (chunk.empty()) break;
        accumulated += chunk;
    }
    return accumulated;
}

void ssl_close_raw(SslHandle& h) {
    if (h.write_fd >= 0) {
        shutdown(h.write_fd, SHUT_WR);
        close(h.write_fd);
        h.write_fd = -1;
    }
    if (h.read_fd >= 0) {
        close(h.read_fd);
        h.read_fd = -1;
    }
    if (h.pid > 0) {
        kill(h.pid, SIGTERM);
        int status;
        // Wait briefly, then force kill if still alive
        struct timespec ts = {0, 100000000}; // 100ms
        nanosleep(&ts, nullptr);
        pid_t result = waitpid(h.pid, &status, WNOHANG);
        if (result == 0) {
            kill(h.pid, SIGKILL);
            waitpid(h.pid, &status, 0);
        }
        h.pid = -1;
    }
}

// ============================================================================
// SSL native class methods
// ============================================================================
static Value ssl_send_native(int argc, Value* argv) {
    // argv[0] = this (SslSocket instance), argv[1] = data
    if (argc < 2) return Value();
    int h = get_handle(argv[0]);
    auto it = g_ssl_handles.find(h);
    if (it == g_ssl_handles.end() || !it->second.valid()) return Value();

    std::string data;
    if (argv[1].is_string()) data = argv[1].as_string()->value;
    else if (argv[1].is_number()) data = std::to_string(safe_int(argv[1].get_number()));
    else return Value();

    int sent = ssl_send_raw(it->second, data);
    return Value(static_cast<double>(sent));
}

static Value ssl_recv_native(int argc, Value* argv) {
    // argv[0] = this, argv[1] = max_bytes (optional)
    if (argc < 1) return Value();
    int h = get_handle(argv[0]);
    auto it = g_ssl_handles.find(h);
    if (it == g_ssl_handles.end() || !it->second.valid()) return Value();

    int max_bytes = 4096;
    int timeout_ms = 10000;
    if (argc >= 2 && argv[1].is_number()) max_bytes = safe_int(argv[1].get_number());
    if (argc >= 3 && argv[2].is_number()) timeout_ms = safe_int(argv[2].get_number());

    std::string data = ssl_recv_raw(it->second, max_bytes, timeout_ms);
    if (data.empty()) return Value();
    return Value(static_cast<Obj*>(get_string_table().intern(data)));
}

static Value ssl_recvline_native(int argc, Value* argv) {
    // Read until \n
    if (argc < 1) return Value();
    int h = get_handle(argv[0]);
    auto it = g_ssl_handles.find(h);
    if (it == g_ssl_handles.end() || !it->second.valid()) return Value();

    std::string line = ssl_recv_until(it->second, "\n", 10000, 65536);
    if (line.empty()) return Value();
    return Value(static_cast<Obj*>(get_string_table().intern(line)));
}

static Value ssl_recvall_native(int argc, Value* argv) {
    // Read until connection closes (or timeout)
    if (argc < 1) return Value();
    int h = get_handle(argv[0]);
    auto it = g_ssl_handles.find(h);
    if (it == g_ssl_handles.end() || !it->second.valid()) return Value();

    int timeout_ms = 5000;
    if (argc >= 2 && argv[1].is_number()) timeout_ms = safe_int(argv[1].get_number());

    std::string data = ssl_recv_all(it->second, timeout_ms, 1048576);
    if (data.empty()) return Value();
    return Value(static_cast<Obj*>(get_string_table().intern(data)));
}

static Value ssl_close_native(int argc, Value* argv) {
    if (argc < 1) return Value();
    int h = get_handle(argv[0]);
    auto it = g_ssl_handles.find(h);
    if (it != g_ssl_handles.end()) {
        ssl_close_raw(it->second);
        g_ssl_handles.erase(it);
    }
    return Value();
}

static Value ssl_peer_native(int argc, Value* argv) {
    if (argc < 1) return Value();
    int h = get_handle(argv[0]);
    auto it = g_ssl_handles.find(h);
    if (it == g_ssl_handles.end()) return Value();
    auto& inst = *argv[0].as_instance();
    auto host_it = inst.fields.find("_host");
    auto port_it = inst.fields.find("_port");
    if (host_it == inst.fields.end() || port_it == inst.fields.end()) return Value();
    std::string peer = host_it->second.as_string()->value + ":" +
                       std::to_string(safe_int(port_it->second.get_number()));
    return Value(static_cast<Obj*>(get_string_table().intern(peer)));
}

static Value ssl_connected_native(int argc, Value* argv) {
    if (argc < 1) return Value(false);
    int h = get_handle(argv[0]);
    auto it = g_ssl_handles.find(h);
    if (it == g_ssl_handles.end()) return Value(false);
    return Value(it->second.valid());
}

// ============================================================================
// Registration
// ============================================================================
void register_ssl_native(VM& vm, ObjMap* net_mod) {
    // Create SslSocket class
    g_ssl_cls = allocate_class("SslSocket");
    g_ssl_cls->methods["send"]      = Value(static_cast<Obj*>(allocate_native(ssl_send_native, "SslSocket.send")));
    g_ssl_cls->methods["recv"]      = Value(static_cast<Obj*>(allocate_native(ssl_recv_native, "SslSocket.recv")));
    g_ssl_cls->methods["recvline"]  = Value(static_cast<Obj*>(allocate_native(ssl_recvline_native, "SslSocket.recvline")));
    g_ssl_cls->methods["recvall"]   = Value(static_cast<Obj*>(allocate_native(ssl_recvall_native, "SslSocket.recvall")));
    g_ssl_cls->methods["close"]     = Value(static_cast<Obj*>(allocate_native(ssl_close_native, "SslSocket.close")));
    g_ssl_cls->methods["peer"]      = Value(static_cast<Obj*>(allocate_native(ssl_peer_native, "SslSocket.peer")));
    g_ssl_cls->methods["connected"] = Value(static_cast<Obj*>(allocate_native(ssl_connected_native, "SslSocket.connected")));
    vm.set_global("SslSocket", Value(static_cast<Obj*>(g_ssl_cls)));

    // net.ssl_available() -> bool
    mod_put(net_mod, "ssl_available", [](int, Value*) -> Value {
        return Value(ssl_available());
    });

    // net.ssl_connect(host, port) -> SslSocket instance
    mod_put(net_mod, "ssl_connect", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_string() || !argv[1].is_number()) return Value();
        std::string host = argv[0].as_string()->value;
        int port = safe_int(argv[1].get_number());

        SslHandle h = ssl_connect_raw(host, port);
        if (!h.valid()) return Value();

        int handle = g_next_ssl_handle++;
        g_ssl_handles[handle] = h;

        auto* inst = allocate_instance(g_ssl_cls);
        inst->fields["_handle"] = Value(static_cast<double>(handle));
        inst->fields["_host"] = Value(static_cast<Obj*>(get_string_table().intern(host)));
        inst->fields["_port"] = Value(static_cast<double>(port));
        return Value(static_cast<Obj*>(inst));
    });
}

} // namespace akar
