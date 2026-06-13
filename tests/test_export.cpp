// Tests for @export variable feature
#include "akar/api/akar.h"

TEST(export_basic_read) {
    akar_api::VM vm;
    auto err = vm.exec("@export let x = 42");
    ASSERT_TRUE(err == AKAR_OK);
    akar_Type t = vm.get_global("x");
    ASSERT_EQ(t, AKAR_TYPE_NUMBER);
    ASSERT_EQ(vm.to_number(-1), 42.0);
    vm.pop(1);
}

TEST(export_basic_write) {
    akar_api::VM vm;
    vm.exec("@export let x = 10");
    vm.exec("x = 20");
    vm.get_global("x");
    ASSERT_EQ(vm.to_number(-1), 20.0);
    vm.pop(1);
}

TEST(export_read_write_roundtrip) {
    akar_api::VM vm;
    vm.exec("@export let health = 100");
    vm.exec("health = health - 10");
    vm.exec("health = health + 5");
    vm.get_global("health");
    ASSERT_EQ(vm.to_number(-1), 95.0);
    vm.pop(1);
}

TEST(export_multiple_variables) {
    akar_api::VM vm;
    vm.exec("@export let a = 1");
    vm.exec("@export let b = 2");
    vm.exec("@export let c = 3");
    vm.exec("c = a + b + c");
    vm.get_global("c");
    ASSERT_EQ(vm.to_number(-1), 6.0);
    vm.pop(1);
}

// Getter/setter callback tests
static double g_host_health = 100.0;

static double test_export_getter(void* userdata) {
    double* val = (double*)userdata;
    return *val;
}

static void test_export_setter(void* userdata, double new_value) {
    double* val = (double*)userdata;
    *val = new_value;
}

TEST(export_getter_callback) {
    g_host_health = 77.0;
    akar_api::VM vm;
    vm.exec("@export let health = 0");
    vm.export_var("health", test_export_getter, test_export_setter, &g_host_health);

    vm.exec("let h = health");
    vm.get_global("h");
    ASSERT_EQ(vm.to_number(-1), 77.0);
    vm.pop(1);
}

TEST(export_setter_callback) {
    g_host_health = 0.0;
    akar_api::VM vm;
    vm.exec("@export let health = 50");
    vm.export_var("health", test_export_getter, test_export_setter, &g_host_health);

    vm.exec("health = 200");
    ASSERT_EQ(g_host_health, 200.0);
}

TEST(export_enumeration) {
    akar_api::VM vm;
    vm.exec("@export let x = 1");
    vm.exec("@export let y = 2");
    vm.exec("@export let z = 3");

    int count = vm.export_count();
    ASSERT_TRUE(count >= 3);
}

TEST(export_string_value) {
    akar_api::VM vm;
    vm.exec("@export let name = \"hello\"");
    vm.exec("name = name + \" world\"");
    vm.get_global("name");
    ASSERT_EQ(std::string(vm.to_string(-1)), "hello world");
    vm.pop(1);
}
