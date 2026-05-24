#pragma once
// Akar Script - HTTP/HTTPS native client
// Pure C++ HTTP/1.1 implementation, uses SSL subprocess for HTTPS

#include <string>
#include <unordered_map>

namespace akar {

class VM;

struct HTTPResponse {
    int status = 0;
    std::string status_text;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

// Raw C++ API
HTTPResponse http_request(const std::string& method, const std::string& url,
                          const std::string& body = "",
                          const std::unordered_map<std::string, std::string>& headers = {});

// Register http module
void register_http_native(VM& vm);

} // namespace akar
