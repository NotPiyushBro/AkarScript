#include "akar/vm/vm.h"
#include "akar/native/ssl.h"
#include "akar/native/http.h"
#include <climits>

static int safe_int(double v) {
    return (v >= INT_MIN && v <= INT_MAX) ? static_cast<int>(v) : 0;
}

// ============================================================================
// SSL module tests
// ============================================================================

TEST(ssl_available_registered) {
    akar::VM vm;
    auto result = vm.interpret("let ok = net.ssl_available()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
    auto ok = vm.get_global("ok");
    ASSERT_TRUE(ok.is_bool());
}

TEST(ssl_connect_class_registered) {
    akar::VM vm;
    auto result = vm.interpret("let t = type(SslSocket)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
    auto t = vm.get_global("t");
    ASSERT_TRUE(t.is_string());
    ASSERT_EQ(std::string(t.as_string()->value), "class");
}

TEST(ssl_connect_to_remote) {
    akar::VM vm;
    // Check if openssl is available first
    bool avail = akar::ssl_available();
    if (!avail) return; // Skip if openssl not installed

    auto result = vm.interpret(
        "let s = net.ssl_connect(\"httpbin.org\", 443)\n"
        "let c = s.connected()\n"
        "let p = s.peer()\n"
        "s.close()\n"
        "let c2 = s.connected()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);

    auto c = vm.get_global("c");
    ASSERT_TRUE(c.is_bool());
    ASSERT_TRUE(c.get_bool());  // Should be connected

    auto p = vm.get_global("p");
    ASSERT_TRUE(p.is_string());
    ASSERT_EQ(std::string(p.as_string()->value), "httpbin.org:443");

    auto c2 = vm.get_global("c2");
    ASSERT_TRUE(c2.is_bool());
    ASSERT_TRUE(!c2.get_bool());  // Should be disconnected after close
}

TEST(ssl_send_recv) {
    bool avail = akar::ssl_available();
    if (!avail) return;

    akar::VM vm;
    auto result = vm.interpret(
        "let s = net.ssl_connect(\"httpbin.org\", 443)\n"
        "s.send(\"GET /ip HTTP/1.1\\r\\nHost: httpbin.org\\r\\nConnection: close\\r\\n\\r\\n\")\n"
        "let resp = s.recvall(8000)\n"
        "let n = len(resp)\n"
        "s.close()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);

    auto n = vm.get_global("n");
    ASSERT_TRUE(n.is_number());
    ASSERT_TRUE(n.get_number() > 0);
}

TEST(ssl_connect_invalid_host) {
    bool avail = akar::ssl_available();
    if (!avail) return;

    akar::VM vm;
    auto result = vm.interpret(
        "let s = net.ssl_connect(\"this.host.definitely.does.not.exist.invalid\", 443)\n"
        "let ok = false\n"
        "if (s != nil) {\n"
        "  ok = s.connected()\n"
        "  s.close()\n"
        "}\n");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
    auto s = vm.get_global("s");
    // Either nil (immediate failure) or not connected (openssl exited after DNS failure)
    auto ok = vm.get_global("ok");
    ASSERT_TRUE(ok.is_bool());
    ASSERT_TRUE(!ok.get_bool());
}

// ============================================================================
// HTTP module tests
// ============================================================================

TEST(http_module_registered) {
    akar::VM vm;
    auto result = vm.interpret(
        "let t = type(http)\n"
        "let has_get = type(http.get)\n"
        "let has_post = type(http.post)\n"
        "let has_put = type(http.put)\n"
        "let has_del = type(http.delete)\n"
        "let has_req = type(http.request)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);

    auto t = vm.get_global("t");
    ASSERT_TRUE(t.is_string());
    ASSERT_EQ(std::string(t.as_string()->value), "map");

    auto has_get = vm.get_global("has_get");
    ASSERT_TRUE(has_get.is_string());
    ASSERT_EQ(std::string(has_get.as_string()->value), "unknown"); // native function
}

TEST(http_get_plain) {
    akar::VM vm;
    auto result = vm.interpret(
        "let r = http.get(\"http://httpbin.org/ip\")\n"
        "let s = r.status\n"
        "let st = r.status_text\n"
        "let bl = len(r.body)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);

    auto s = vm.get_global("s");
    ASSERT_TRUE(s.is_number());
    ASSERT_EQ(safe_int(s.get_number()), 200);

    auto st = vm.get_global("st");
    ASSERT_TRUE(st.is_string());
    ASSERT_EQ(std::string(st.as_string()->value), "OK");

    auto bl = vm.get_global("bl");
    ASSERT_TRUE(bl.is_number());
    ASSERT_TRUE(bl.get_number() > 0);
}

TEST(http_get_https) {
    akar::VM vm;
    bool avail = akar::ssl_available();
    if (!avail) return;

    auto result = vm.interpret(
        "let r = http.get(\"https://httpbin.org/ip\")\n"
        "let s = r.status\n"
        "let bl = len(r.body)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);

    auto s = vm.get_global("s");
    ASSERT_TRUE(s.is_number());
    ASSERT_EQ(safe_int(s.get_number()), 200);

    auto bl = vm.get_global("bl");
    ASSERT_TRUE(bl.is_number());
    ASSERT_TRUE(bl.get_number() > 0);
}

TEST(http_post_json) {
    akar::VM vm;
    bool avail = akar::ssl_available();
    if (!avail) return;

    auto result = vm.interpret(
        "let r = http.post(\"https://httpbin.org/post\", \"{\\\"name\\\":\\\"akar\\\"}\", {\"Content-Type\": \"application/json\"})\n"
        "let s = r.status\n"
        "let bl = len(r.body)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);

    auto s = vm.get_global("s");
    ASSERT_TRUE(s.is_number());
    ASSERT_EQ(safe_int(s.get_number()), 200);

    auto bl = vm.get_global("bl");
    ASSERT_TRUE(bl.is_number());
    ASSERT_TRUE(bl.get_number() > 0);
}

TEST(http_response_has_headers) {
    akar::VM vm;
    auto result = vm.interpret(
        "let r = http.get(\"http://httpbin.org/ip\")\n"
        "let ht = type(r.headers)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);

    auto ht = vm.get_global("ht");
    ASSERT_TRUE(ht.is_string());
    ASSERT_EQ(std::string(ht.as_string()->value), "map");
}

TEST(http_custom_headers) {
    akar::VM vm;
    auto result = vm.interpret(
        "let r = http.get(\"http://httpbin.org/headers\", {\"X-Akar\": \"test123\"})\n"
        "let s = r.status");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);

    auto s = vm.get_global("s");
    ASSERT_TRUE(s.is_number());
    ASSERT_EQ(safe_int(s.get_number()), 200);
}

TEST(http_request_method) {
    akar::VM vm;
    auto result = vm.interpret(
        "let r = http.request(\"GET\", \"http://httpbin.org/ip\")\n"
        "let s = r.status");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);

    auto s = vm.get_global("s");
    ASSERT_TRUE(s.is_number());
    ASSERT_EQ(safe_int(s.get_number()), 200);
}

TEST(http_invalid_url) {
    akar::VM vm;
    auto result = vm.interpret(
        "let r = http.get(\"http://this.host.definitely.does.not.exist:12345/\")\n"
        "let s = r.status");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);

    auto s = vm.get_global("s");
    ASSERT_TRUE(s.is_number());
    ASSERT_EQ(safe_int(s.get_number()), -1);  // Connection failed
}

// ============================================================================
// C++ API tests
// ============================================================================

TEST(cpp_ssl_available) {
    // Just verify it doesn't crash
    bool avail = akar::ssl_available();
    // Either true or false, but shouldn't crash
    ASSERT_TRUE(avail || !avail);
}

TEST(cpp_http_request) {
    akar::HTTPResponse resp = akar::http_request("GET", "http://httpbin.org/ip");
    ASSERT_TRUE(resp.status == 200);
    ASSERT_TRUE(!resp.body.empty());
    ASSERT_TRUE(resp.status_text == "OK");
}

TEST(cpp_http_request_https) {
    if (!akar::ssl_available()) return;
    akar::HTTPResponse resp = akar::http_request("GET", "https://httpbin.org/ip");
    ASSERT_TRUE(resp.status == 200);
    ASSERT_TRUE(!resp.body.empty());
}

TEST(cpp_ssl_connect_raw) {
    if (!akar::ssl_available()) return;
    akar::SslHandle h = akar::ssl_connect_raw("httpbin.org", 443);
    ASSERT_TRUE(h.valid());

    int sent = akar::ssl_send_raw(h, "GET /ip HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n");
    ASSERT_TRUE(sent > 0);

    std::string resp = akar::ssl_recv_all(h, 10000, 65536);
    ASSERT_TRUE(!resp.empty());
    ASSERT_TRUE(resp.find("200") != std::string::npos);

    akar::ssl_close_raw(h);
    ASSERT_TRUE(!h.valid());
}
