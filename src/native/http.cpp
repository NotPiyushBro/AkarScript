// Akar Script - HTTP/HTTPS native client
// Pure C++ HTTP/1.1 implementation
// Uses raw TCP sockets for HTTP, SSL subprocess for HTTPS

#include "akar/native/http.h"
#include "akar/native/ssl.h"
#include "akar/vm/vm.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <climits>
#include <cstring>
#include <cstdlib>
#include <string>
#include <sstream>
#include <algorithm>
#include <vector>
#include <unordered_map>

namespace akar {

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

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}

// ============================================================================
// URL Parsing
// ============================================================================
struct URL {
    std::string scheme;
    std::string host;
    int port = 80;
    std::string path;
    bool ssl = false;
};

static bool parse_url(const std::string& url_str, URL& url) {
    std::string s = url_str;
    // Scheme
    size_t pos = s.find("://");
    if (pos != std::string::npos) {
        url.scheme = to_lower(s.substr(0, pos));
        s = s.substr(pos + 3);
    } else {
        url.scheme = "http";
    }
    url.ssl = (url.scheme == "https");
    url.port = url.ssl ? 443 : 80;

    // Path
    pos = s.find('/');
    std::string host_port;
    if (pos != std::string::npos) {
        host_port = s.substr(0, pos);
        url.path = s.substr(pos);
    } else {
        host_port = s;
        url.path = "/";
    }

    // Host:port
    pos = host_port.find(':');
    if (pos != std::string::npos) {
        url.host = host_port.substr(0, pos);
        url.port = std::atoi(host_port.substr(pos + 1).c_str());
    } else {
        url.host = host_port;
    }

    return !url.host.empty();
}

// ============================================================================
// TCP connect helper
// ============================================================================
static int tcp_connect(const std::string& host, int port) {
    struct addrinfo hints{}, *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    std::string port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0) return -1;

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }

    // Set connect timeout via non-blocking + select
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    int rc = ::connect(fd, res->ai_addr, res->ai_addrlen);
    if (rc < 0 && errno != EINPROGRESS) {
        ::close(fd);
        freeaddrinfo(res);
        return -1;
    }
    if (rc < 0) {
        // Wait for connect with timeout
        fd_set wfds;
        struct timeval tv;
        FD_ZERO(&wfds);
        FD_SET(fd, &wfds);
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        rc = select(fd + 1, NULL, &wfds, NULL, &tv);
        if (rc <= 0) { ::close(fd); freeaddrinfo(res); return -1; }
        int err = 0;
        socklen_t len = sizeof(err);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
        if (err != 0) { ::close(fd); freeaddrinfo(res); return -1; }
    }
    // Set back to blocking
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);
    freeaddrinfo(res);
    return fd;
}

static int tcp_send(int fd, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = write(fd, data.c_str() + sent, data.size() - sent);
        if (n <= 0) return -1;
        sent += n;
    }
    return static_cast<int>(sent);
}

static std::string tcp_recv(int fd, int max_bytes, int timeout_ms) {
    fd_set fds;
    struct timeval tv;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (select(fd + 1, &fds, NULL, NULL, &tv) <= 0) return "";

    std::vector<char> buf(max_bytes);
    ssize_t n = read(fd, buf.data(), max_bytes);
    if (n <= 0) return "";
    return std::string(buf.data(), n);
}

static std::string tcp_recv_until(int fd, const std::string& delim, int timeout_ms, int max_bytes) {
    std::string accumulated;
    while (static_cast<int>(accumulated.size()) < max_bytes) {
        std::string chunk = tcp_recv(fd, 4096, timeout_ms);
        if (chunk.empty()) break;
        accumulated += chunk;
        if (accumulated.find(delim) != std::string::npos) break;
    }
    return accumulated;
}

static std::string tcp_recv_all(int fd, int timeout_ms, int max_bytes) {
    std::string accumulated;
    while (static_cast<int>(accumulated.size()) < max_bytes) {
        std::string chunk = tcp_recv(fd, 8192, timeout_ms);
        if (chunk.empty()) break;
        accumulated += chunk;
    }
    return accumulated;
}

// ============================================================================
// Chunked transfer decoding
// ============================================================================
static std::string decode_chunked(const std::string& data) {
    std::string result;
    size_t pos = 0;
    while (pos < data.size()) {
        // Find chunk size line
        size_t line_end = data.find("\r\n", pos);
        if (line_end == std::string::npos) break;
        std::string size_str = data.substr(pos, line_end - pos);
        // Remove extensions (;...)
        size_t semi = size_str.find(';');
        if (semi != std::string::npos) size_str = size_str.substr(0, semi);
        size_t chunk_size = 0;
        try { chunk_size = std::stoul(size_str, nullptr, 16); } catch (...) { break; }
        if (chunk_size == 0) break; // Last chunk
        pos = line_end + 2;
        if (pos + chunk_size > data.size()) break;
        result.append(data, pos, chunk_size);
        pos += chunk_size + 2; // skip data + trailing \r\n
    }
    return result;
}

// ============================================================================
// Parse HTTP response
// ============================================================================
static bool parse_response(const std::string& raw, HTTPResponse& resp) {
    // Find end of headers
    size_t header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        // Try \n\n
        header_end = raw.find("\n\n");
        if (header_end == std::string::npos) return false;
        // Treat as single \n line endings
        std::string header_part = raw.substr(0, header_end);
        std::string body_part = raw.substr(header_end + 2);

        // Parse status line
        size_t nl = header_part.find('\n');
        if (nl == std::string::npos) return false;
        std::string status_line = trim(header_part.substr(0, nl));
        header_part = header_part.substr(nl + 1);

        // Parse "HTTP/x.y STATUS TEXT"
        size_t sp1 = status_line.find(' ');
        if (sp1 == std::string::npos) return false;
        size_t sp2 = status_line.find(' ', sp1 + 1);
        if (sp2 == std::string::npos) {
            resp.status = std::atoi(status_line.substr(sp1 + 1).c_str());
            resp.status_text = "";
        } else {
            resp.status = std::atoi(status_line.substr(sp1 + 1, sp2 - sp1 - 1).c_str());
            resp.status_text = status_line.substr(sp2 + 1);
        }

        // Parse headers
        std::istringstream hs(header_part);
        std::string line;
        while (std::getline(hs, line)) {
            if (line.empty() || line == "\r") continue;
            if (line.back() == '\r') line.pop_back();
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = trim(line.substr(0, colon));
                std::string val = trim(line.substr(colon + 1));
                resp.headers[to_lower(key)] = val;
            }
        }
        resp.body = body_part;
        return true;
    }

    std::string header_part = raw.substr(0, header_end);
    std::string body_part = raw.substr(header_end + 4);

    // Parse status line
    size_t nl = header_part.find("\r\n");
    if (nl == std::string::npos) nl = header_part.find('\n');
    if (nl == std::string::npos) return false;
    std::string status_line = header_part.substr(0, nl);
    header_part = header_part.substr(nl + (header_part[nl] == '\r' ? 2 : 1));

    // Parse "HTTP/x.y STATUS TEXT"
    size_t sp1 = status_line.find(' ');
    if (sp1 == std::string::npos) return false;
    size_t sp2 = status_line.find(' ', sp1 + 1);
    if (sp2 == std::string::npos) {
        resp.status = std::atoi(status_line.substr(sp1 + 1).c_str());
        resp.status_text = "";
    } else {
        resp.status = std::atoi(status_line.substr(sp1 + 1, sp2 - sp1 - 1).c_str());
        resp.status_text = status_line.substr(sp2 + 1);
    }

    // Parse headers
    std::istringstream hs(header_part);
    std::string line;
    while (std::getline(hs, line)) {
        if (line.empty() || line == "\r") continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = trim(line.substr(0, colon));
            std::string val = trim(line.substr(colon + 1));
            resp.headers[to_lower(key)] = val;
        }
    }

    // Handle chunked encoding
    auto te = resp.headers.find("transfer-encoding");
    if (te != resp.headers.end() && te->second.find("chunked") != std::string::npos) {
        resp.body = decode_chunked(body_part);
    } else {
        resp.body = body_part;
    }

    return true;
}

// ============================================================================
// Core HTTP request
// ============================================================================
HTTPResponse http_request(const std::string& method, const std::string& url_str,
                          const std::string& body,
                          const std::unordered_map<std::string, std::string>& extra_headers) {
    HTTPResponse resp;
    URL url;
    if (!parse_url(url_str, url)) { resp.status = -1; resp.status_text = "Invalid URL"; return resp; }

    bool use_ssl = url.ssl;

    // Connect
    if (use_ssl) {
        if (!ssl_available()) { resp.status = -1; resp.status_text = "SSL not available (openssl not found)"; return resp; }
        SslHandle ssl = ssl_connect_raw(url.host, url.port);
        if (!ssl.valid()) { resp.status = -1; resp.status_text = "SSL connect failed"; return resp; }

        // Build request
        std::string req = method + " " + url.path + " HTTP/1.1\r\n";
        req += "Host: " + url.host + "\r\n";
        req += "Connection: close\r\n";
        if (!body.empty()) req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        for (auto& [k, v] : extra_headers) {
            if (to_lower(k) != "host" && to_lower(k) != "connection" && to_lower(k) != "content-length")
                req += k + ": " + v + "\r\n";
        }
        req += "\r\n";
        req += body;

        ssl_send_raw(ssl, req);

        // Read full response
        std::string raw = ssl_recv_all(ssl, 15000, 1048576);
        ssl_close_raw(ssl);

        if (!parse_response(raw, resp)) {
            resp.status = -1;
            resp.status_text = "Failed to parse response";
        }
    } else {
        int fd = tcp_connect(url.host, url.port);
        if (fd < 0) { resp.status = -1; resp.status_text = "TCP connect failed"; return resp; }

        // Build request
        std::string req = method + " " + url.path + " HTTP/1.1\r\n";
        req += "Host: " + url.host + "\r\n";
        req += "Connection: close\r\n";
        if (!body.empty()) req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        for (auto& [k, v] : extra_headers) {
            if (to_lower(k) != "host" && to_lower(k) != "connection" && to_lower(k) != "content-length")
                req += k + ": " + v + "\r\n";
        }
        req += "\r\n";
        req += body;

        tcp_send(fd, req);

        // Read full response
        std::string raw = tcp_recv_all(fd, 15000, 1048576);
        ::close(fd);

        if (!parse_response(raw, resp)) {
            resp.status = -1;
            resp.status_text = "Failed to parse response";
        }
    }

    return resp;
}

// ============================================================================
// Parse headers from Akar map value
// ============================================================================
static std::unordered_map<std::string, std::string> parse_akar_headers(Value hdrs) {
    std::unordered_map<std::string, std::string> result;
    if (!hdrs.is_map()) return result;
    for (auto& [k, v] : hdrs.as_map()->entries) {
        if (v.is_string()) result[k] = v.as_string()->value;
        else if (v.is_number()) result[k] = std::to_string(safe_int(v.get_number()));
    }
    return result;
}

// ============================================================================
// Build Akar response map from HTTPResponse
// ============================================================================
static Value make_response_map(VM& vm, const HTTPResponse& resp) {
    auto* m = allocate_map();
    m->entries["status"] = Value(static_cast<double>(resp.status));
    m->entries["status_text"] = Value(static_cast<Obj*>(get_string_table().intern(resp.status_text)));
    m->entries["body"] = Value(static_cast<Obj*>(get_string_table().intern(resp.body)));

    // Build headers map
    auto* hdrs = allocate_map();
    for (auto& [k, v] : resp.headers) {
        hdrs->entries[k] = Value(static_cast<Obj*>(get_string_table().intern(v)));
    }
    m->entries["headers"] = Value(static_cast<Obj*>(hdrs));

    return Value(static_cast<Obj*>(m));
}

// ============================================================================
// Registration
// ============================================================================
void register_http_native(VM& vm) {
    auto* http = allocate_map();

    // http.request(method, url, body?, headers?) -> response
    mod_put(http, "request", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_string() || !argv[1].is_string()) return Value();
        std::string method = argv[0].as_string()->value;
        std::string url = argv[1].as_string()->value;
        std::string body;
        std::unordered_map<std::string, std::string> hdrs;
        if (argc >= 3 && argv[2].is_string()) body = argv[2].as_string()->value;
        if (argc >= 4) hdrs = parse_akar_headers(argv[3]);
        HTTPResponse resp = http_request(method, url, body, hdrs);
        return make_response_map(*static_cast<VM*>(nullptr), resp);
    });

    // http.get(url, headers?) -> response
    mod_put(http, "get", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        std::string url = argv[0].as_string()->value;
        std::unordered_map<std::string, std::string> hdrs;
        if (argc >= 2) hdrs = parse_akar_headers(argv[1]);
        HTTPResponse resp = http_request("GET", url, "", hdrs);
        return make_response_map(*static_cast<VM*>(nullptr), resp);
    });

    // http.post(url, body?, headers?) -> response
    mod_put(http, "post", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        std::string url = argv[0].as_string()->value;
        std::string body;
        std::unordered_map<std::string, std::string> hdrs;
        if (argc >= 2 && argv[1].is_string()) body = argv[1].as_string()->value;
        if (argc >= 3) hdrs = parse_akar_headers(argv[2]);
        HTTPResponse resp = http_request("POST", url, body, hdrs);
        return make_response_map(*static_cast<VM*>(nullptr), resp);
    });

    // http.put(url, body?, headers?) -> response
    mod_put(http, "put", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        std::string url = argv[0].as_string()->value;
        std::string body;
        std::unordered_map<std::string, std::string> hdrs;
        if (argc >= 2 && argv[1].is_string()) body = argv[1].as_string()->value;
        if (argc >= 3) hdrs = parse_akar_headers(argv[2]);
        HTTPResponse resp = http_request("PUT", url, body, hdrs);
        return make_response_map(*static_cast<VM*>(nullptr), resp);
    });

    // http.delete(url, headers?) -> response
    mod_put(http, "delete", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        std::string url = argv[0].as_string()->value;
        std::unordered_map<std::string, std::string> hdrs;
        if (argc >= 2) hdrs = parse_akar_headers(argv[1]);
        HTTPResponse resp = http_request("DELETE", url, "", hdrs);
        return make_response_map(*static_cast<VM*>(nullptr), resp);
    });

    // http.head(url, headers?) -> response
    mod_put(http, "head", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        std::string url = argv[0].as_string()->value;
        std::unordered_map<std::string, std::string> hdrs;
        if (argc >= 2) hdrs = parse_akar_headers(argv[1]);
        HTTPResponse resp = http_request("HEAD", url, "", hdrs);
        return make_response_map(*static_cast<VM*>(nullptr), resp);
    });

    vm.set_global("http", Value(static_cast<Obj*>(http)));
}

} // namespace akar
