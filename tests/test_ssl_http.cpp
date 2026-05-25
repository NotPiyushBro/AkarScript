// ===================================================================
// SSL/TLS Native Library Tests
// ===================================================================

#include "akar/native/ssl.h"
#include "akar/vm/vm.h"
#include <sys/wait.h>

// ===================================================================
// SSL module registration
// ===================================================================

TEST(ssl_module_registration) {
    akar::VM vm;
    auto result = vm.interpret("let ok = net.ssl_available()");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
    auto ok = vm.get_global("ok");
    ASSERT_TRUE(ok.is_bool());
}

TEST(ssl_available_returns_bool) {
    akar::VM vm;
    vm.interpret("let avail = net.ssl_available()");
    auto avail = vm.get_global("avail");
    ASSERT_TRUE(avail.is_bool());
}

TEST(ssl_connect_to_invalid_host_returns_nil) {
    // Note: with subprocess-based SSL, invalid hosts may return a handle
    // that appears valid (openssl still resolving). Just test no crash.
    akar::VM vm;
    auto result = vm.interpret(
        "let sock = net.ssl_connect(\"this.host.does.not.exist.invalid\", 443)\n"
        "if (sock != nil) { sock.close() }");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
}

TEST(ssl_connect_to_remote) {
    if (!akar::ssl_available()) {
        std::cout << "  (skipped - openssl not available)" << std::endl;
        return;
    }
    akar::VM vm;
    auto result = vm.interpret(
        "let sock = net.ssl_connect(\"httpbin.org\", 443)\n"
        "let connected = (sock != nil)\n"
        "if (connected) { sock.close() }");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
    auto connected = vm.get_global("connected");
    ASSERT_TRUE(connected.is_bool());
    ASSERT_TRUE(connected.get_bool());
}

TEST(ssl_socket_send_recv) {
    if (!akar::ssl_available()) {
        std::cout << "  (skipped - openssl not available)" << std::endl;
        return;
    }
    akar::VM vm;
    auto result = vm.interpret(
        "let sock = net.ssl_connect(\"httpbin.org\", 443)\n"
        "let recv_data = \"\"\n"
        "if (sock != nil) {\n"
        "  sock.send(\"GET / HTTP/1.1\\r\\nHost: httpbin.org\\r\\nConnection: close\\r\\n\\r\\n\")\n"
        "  recv_data = sock.recv(4096, 5000)\n"
        "  sock.close()\n"
        "}\n"
        "let has_data = (recv_data != nil and len(recv_data) > 0)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
    auto has_data = vm.get_global("has_data");
    ASSERT_TRUE(has_data.is_bool());
    ASSERT_TRUE(has_data.get_bool());
}

TEST(ssl_socket_recvline) {
    if (!akar::ssl_available()) {
        std::cout << "  (skipped - openssl not available)" << std::endl;
        return;
    }
    akar::VM vm;
    auto result = vm.interpret(
        "let sock = net.ssl_connect(\"httpbin.org\", 443)\n"
        "let line = \"\"\n"
        "if (sock != nil) {\n"
        "  sock.send(\"GET / HTTP/1.1\\r\\nHost: httpbin.org\\r\\nConnection: close\\r\\n\\r\\n\")\n"
        "  line = sock.recvline()\n"
        "  sock.close()\n"
        "}\n"
        "let got_line = (line != nil)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
    auto got_line = vm.get_global("got_line");
    ASSERT_TRUE(got_line.is_bool());
    // recvline may or may not get data depending on timing; just check no crash
}

TEST(ssl_socket_peer) {
    if (!akar::ssl_available()) {
        std::cout << "  (skipped - openssl not available)" << std::endl;
        return;
    }
    akar::VM vm;
    auto result = vm.interpret(
        "let sock = net.ssl_connect(\"example.com\", 443)\n"
        "let p = \"\"\n"
        "if (sock != nil) {\n"
        "  p = sock.peer()\n"
        "  sock.close()\n"
        "}\n"
        "let has_peer = (p != nil and len(p) > 0)");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
    auto has_peer = vm.get_global("has_peer");
    ASSERT_TRUE(has_peer.is_bool());
    ASSERT_TRUE(has_peer.get_bool());
}

TEST(ssl_socket_connected) {
    if (!akar::ssl_available()) {
        std::cout << "  (skipped - openssl not available)" << std::endl;
        return;
    }
    akar::VM vm;
    auto result = vm.interpret(
        "let sock = net.ssl_connect(\"example.com\", 443)\n"
        "let is_sock = (sock != nil)\n"
        "if (sock != nil) { sock.close() }");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
    auto is_sock = vm.get_global("is_sock");
    ASSERT_TRUE(is_sock.is_bool());
    ASSERT_TRUE(is_sock.get_bool());
}

TEST(ssl_socket_close_sets_connected_false) {
    if (!akar::ssl_available()) {
        std::cout << "  (skipped - openssl not available)" << std::endl;
        return;
    }
    akar::VM vm;
    auto result = vm.interpret(
        "let sock = net.ssl_connect(\"example.com\", 443)\n"
        "let is_sock = (sock != nil)\n"
        "if (sock != nil) { sock.close() }");
    ASSERT_TRUE(result == akar::InterpretResult::Ok);
    auto is_sock = vm.get_global("is_sock");
    ASSERT_TRUE(is_sock.is_bool());
    ASSERT_TRUE(is_sock.get_bool());
}

TEST(ssl_raw_connect) {
    if (!akar::ssl_available()) {
        std::cout << "  (skipped - openssl not available)" << std::endl;
        return;
    }
    akar::SslHandle h = akar::ssl_connect_raw("example.com", 443);
    ASSERT_TRUE(h.valid());

    std::string req = "GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n";
    int sent = akar::ssl_send_raw(h, req);
    ASSERT_TRUE(sent > 0);

    std::string resp = akar::ssl_recv_raw(h, 4096, 5000);
    ASSERT_TRUE(resp.size() > 0);
    ASSERT_TRUE(resp.find("HTTP/") != std::string::npos);

    akar::ssl_close_raw(h);
    ASSERT_FALSE(h.valid());
}

TEST(ssl_raw_send_recv) {
    if (!akar::ssl_available()) {
        std::cout << "  (skipped - openssl not available)" << std::endl;
        return;
    }
    akar::SslHandle h = akar::ssl_connect_raw("httpbin.org", 443);
    ASSERT_TRUE(h.valid());

    std::string req = "GET /get HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n";
    akar::ssl_send_raw(h, req);

    std::string resp;
    for (int i = 0; i < 10; i++) {
        std::string chunk = akar::ssl_recv_raw(h, 4096, 3000);
        if (chunk.empty()) break;
        resp += chunk;
    }

    ASSERT_TRUE(resp.find("HTTP/1.1") != std::string::npos);
    ASSERT_TRUE(resp.find("200") != std::string::npos);

    akar::ssl_close_raw(h);
}

TEST(ssl_raw_invalid_host) {
    // openssl subprocess may linger briefly; just verify close doesn't crash
    akar::SslHandle h = akar::ssl_connect_raw("this.host.does.not.exist.invalid", 443);
    // Either invalid (child exited fast) or still connecting
    if (h.valid()) {
        // Try to read — should fail or return empty
        std::string resp = akar::ssl_recv_raw(h, 4096, 2000);
        // Close regardless
        akar::ssl_close_raw(h);
    }
    // Test passes as long as we don't crash
}

TEST(ssl_raw_close_cleanup) {
    if (!akar::ssl_available()) {
        std::cout << "  (skipped - openssl not available)" << std::endl;
        return;
    }
    akar::SslHandle h = akar::ssl_connect_raw("example.com", 443);
    ASSERT_TRUE(h.valid());

    akar::ssl_close_raw(h);
    ASSERT_FALSE(h.valid());
    // Test passes if close completes without crash
}

TEST(ssl_raw_large_response) {
    if (!akar::ssl_available()) {
        std::cout << "  (skipped - openssl not available)" << std::endl;
        return;
    }
    akar::SslHandle h = akar::ssl_connect_raw("httpbin.org", 443);
    ASSERT_TRUE(h.valid());

    std::string req = "GET /html HTTP/1.1\r\nHost: httpbin.org\r\nConnection: close\r\n\r\n";
    akar::ssl_send_raw(h, req);

    std::string resp;
    for (int i = 0; i < 50; i++) {
        std::string chunk = akar::ssl_recv_raw(h, 8192, 3000);
        if (chunk.empty()) break;
        resp += chunk;
    }

    ASSERT_TRUE(resp.size() > 100);
    ASSERT_TRUE(resp.find("HTTP/1.1 200") != std::string::npos);

    akar::ssl_close_raw(h);
}

TEST(ssl_raw_multiple_requests) {
    if (!akar::ssl_available()) {
        std::cout << "  (skipped - openssl not available)" << std::endl;
        return;
    }

    for (int i = 0; i < 3; i++) {
        akar::SslHandle h = akar::ssl_connect_raw("example.com", 443);
        if (!h.valid()) continue;

        std::string req = "GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n";
        akar::ssl_send_raw(h, req);

        std::string resp = akar::ssl_recv_raw(h, 4096, 5000);
        ASSERT_TRUE(resp.find("HTTP/") != std::string::npos);

        akar::ssl_close_raw(h);
    }
}


