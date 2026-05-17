#include <iostream>
#include <string>
#include <functional>
#include <vector>

// Simple test framework
struct TestCase {
    std::string name;
    std::function<void()> func;
};

static std::vector<TestCase> g_tests;
static int g_passed = 0;
static int g_failed = 0;

#define TEST(name) \
    static void test_##name(); \
    static struct Register_##name { \
        Register_##name() { g_tests.push_back({#name, test_##name}); } \
    } reg_##name; \
    static void test_##name()

#define ASSERT_EQ(a, b) do { \
    auto _a = (a); auto _b = (b); \
    if (_a != _b) { \
        std::cerr << "  FAIL: " << #a << " != " << #b << std::endl; \
        g_failed++; return; \
    } \
} while(0)

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        std::cerr << "  FAIL: " << #expr << " is false" << std::endl; \
        g_failed++; return; \
    } \
} while(0)

#define ASSERT_FALSE(expr) do { \
    if ((expr)) { \
        std::cerr << "  FAIL: " << #expr << " is true" << std::endl; \
        g_failed++; return; \
    } \
} while(0)

// Include test files
#include "test_lexer.cpp"
#include "test_parser.cpp"
#include "test_vm.cpp"
#include "test_native.cpp"

int main() {
    std::cout << "Running Akar Script tests..." << std::endl;
    for (auto& test : g_tests) {
        std::cout << "  " << test.name << "... ";
        int before = g_failed;
        test.func();
        if (g_failed == before) {
            std::cout << "PASS" << std::endl;
            g_passed++;
        } else {
            std::cout << "FAIL" << std::endl;
        }
    }
    std::cout << std::endl;
    std::cout << g_passed << " passed, " << g_failed << " failed" << std::endl;
    return g_failed > 0 ? 1 : 0;
}
