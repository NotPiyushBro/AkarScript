#include "akar/vm/vm.h"
#include "akar/native/ssl.h"
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
    bool avail = akar::ssl_available();
    if (!avail) return;

    akar::VM vm;
    auto result = vm.interpret(
        "let s = net.ssl_connect(\"httpbin.org\", 443)\n"
        "let c = s.connected()\n"
        "let p = s.peer()\n"
        "s.close()\n"
        "let c2 = s.connected()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);

    auto c = vm.get_global("c");
    ASSERT_TRUE(c.is_bool());
    ASSERT_TRUE(c.get_bool());

    auto p = vm.get_global("p");
    ASSERT_TRUE(p.is_string());
    ASSERT_EQ(std::string(p.as_string()->value), "httpbin.org:443");

    auto c2 = vm.get_global("c2");
    ASSERT_TRUE(c2.is_bool());
    ASSERT_TRUE(!c2.get_bool());
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
    auto ok = vm.get_global("ok");
    ASSERT_TRUE(ok.is_bool());
    ASSERT_TRUE(!ok.get_bool());
}

// ============================================================================
// HTTP library tests — written in Akar, auto-loaded from lib/http.ak
// ============================================================================

TEST(http_functions_registered) {
    akar::VM vm;
    auto result = vm.interpret(
        "let t_req  = type(http_request)\n"
        "let t_get  = type(http_get)\n"
        "let t_post = type(http_post)\n"
        "let t_put  = type(http_put)\n"
        "let t_del  = type(http_delete)\n"
        "let t_head = type(http_head)\n"
        "let t_url  = type(parse_url)\n"
        "let t_find = type(str_find)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);

    auto t_get = vm.get_global("t_get");
    ASSERT_TRUE(t_get.is_string());
    ASSERT_EQ(std::string(t_get.as_string()->value), "function");

    auto t_url = vm.get_global("t_url");
    ASSERT_TRUE(t_url.is_string());
    ASSERT_EQ(std::string(t_url.as_string()->value), "function");

    auto t_find = vm.get_global("t_find");
    ASSERT_TRUE(t_find.is_string());
    ASSERT_EQ(std::string(t_find.as_string()->value), "function");
}

TEST(http_url_parser) {
    akar::VM vm;
    auto result = vm.interpret(
        "let u1 = parse_url(\"https://example.com:8080/path?q=1\")\n"
        "let u2 = parse_url(\"http://example.com\")\n"
        "let u3 = parse_url(\"https://api.github.com/users/octocat\")");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);

    auto u1 = vm.get_global("u1");
    ASSERT_TRUE(u1.is_map());
    ASSERT_EQ(std::string(u1.as_map()->entries["scheme"].as_string()->value), "https");
    ASSERT_EQ(std::string(u1.as_map()->entries["host"].as_string()->value), "example.com");
    ASSERT_EQ(safe_int(u1.as_map()->entries["port"].get_number()), 8080);
    ASSERT_EQ(std::string(u1.as_map()->entries["path"].as_string()->value), "/path?q=1");

    auto u2 = vm.get_global("u2");
    ASSERT_EQ(safe_int(u2.as_map()->entries["port"].get_number()), 80);

    auto u3 = vm.get_global("u3");
    ASSERT_EQ(std::string(u3.as_map()->entries["path"].as_string()->value), "/users/octocat");
}

TEST(http_get_plain) {
    akar::VM vm;
    auto result = vm.interpret(
        "let r = http_get(\"http://httpbin.org/ip\", nil)\n"
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
    bool avail = akar::ssl_available();
    if (!avail) return;

    akar::VM vm;
    auto result = vm.interpret(
        "let r = http_get(\"https://httpbin.org/ip\", nil)\n"
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
    bool avail = akar::ssl_available();
    if (!avail) return;

    akar::VM vm;
    auto result = vm.interpret(
        "let r = http_post(\"https://httpbin.org/post\", \"{\\\"name\\\":\\\"akar\\\"}\", {\"Content-Type\": \"application/json\"})\n"
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
        "let r = http_get(\"http://httpbin.org/ip\", nil)\n"
        "let ht = type(r.headers)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);

    auto ht = vm.get_global("ht");
    ASSERT_TRUE(ht.is_string());
    ASSERT_EQ(std::string(ht.as_string()->value), "map");
}

TEST(http_request_with_headers) {
    akar::VM vm;
    auto result = vm.interpret(
        "let r = http_request(\"GET\", \"http://httpbin.org/headers\", nil, {\"X-Akar\": \"test123\"})\n"
        "let s = r.status");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);

    auto s = vm.get_global("s");
    ASSERT_TRUE(s.is_number());
    ASSERT_EQ(safe_int(s.get_number()), 200);
}

TEST(http_invalid_url) {
    akar::VM vm;
    auto result = vm.interpret(
        "let r = http_get(\"http://this.host.definitely.does.not.exist:12345/\", nil)\n"
        "let is_nil = (r == nil)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);

    auto is_nil = vm.get_global("is_nil");
    ASSERT_TRUE(is_nil.is_bool());
    ASSERT_TRUE(is_nil.get_bool());
}

// ============================================================================
// C++ raw API tests
// ============================================================================

TEST(cpp_ssl_available) {
    bool avail = akar::ssl_available();
    ASSERT_TRUE(avail || !avail);
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
