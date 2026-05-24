#include "akar/vm/vm.h"
#include "akar/vm/native.h"
#include "akar/vm/system.h"
#include <string>
#include <unistd.h>

// ===== System Library Tests =====

TEST(sys_os_returns_string) {
    akar::VM vm;
    auto r = vm.interpret("let v = sys.os()");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
    // Should be "linux", "darwin", etc.
    ASSERT_TRUE(v.as_string()->value.size() > 0);
}

TEST(sys_arch_returns_string) {
    akar::VM vm;
    auto r = vm.interpret("let v = sys.arch()");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
}

TEST(sys_hostname_returns_string) {
    akar::VM vm;
    auto r = vm.interpret("let v = sys.hostname()");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
}

TEST(sys_cpu_count_returns_number) {
    akar::VM vm;
    auto r = vm.interpret("let v = sys.cpu_count()");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_number());
    ASSERT_TRUE(v.get_number() >= 1);
}

TEST(sys_pid_returns_number) {
    akar::VM vm;
    auto r = vm.interpret("let v = sys.pid()");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_number());
    ASSERT_TRUE(v.get_number() > 0);
}

TEST(sys_now_returns_string) {
    akar::VM vm;
    auto r = vm.interpret("let v = sys.now()");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
    // Should contain a dash (date format YYYY-MM-DD...)
    ASSERT_TRUE(v.as_string()->value.find('-') != std::string::npos);
}

TEST(sys_env_set_and_get) {
    akar::VM vm;
    auto r = vm.interpret(
        "sys.set_env(\"AKAR_TEST_VAR\", \"hello42\")\n"
        "let v = sys.env(\"AKAR_TEST_VAR\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
    ASSERT_EQ(v.as_string()->value, "hello42");
}

TEST(sys_env_nonexistent_returns_nil) {
    akar::VM vm;
    auto r = vm.interpret("let v = sys.env(\"AKAR_NONEXISTENT_VAR_12345\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_nil());
}

TEST(sys_exec_returns_output) {
    akar::VM vm;
    auto r = vm.interpret("let v = sys.exec(\"echo hello_exec_test\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_map());
    auto& entries = v.as_map()->entries;
    auto it = entries.find("stdout");
    ASSERT_TRUE(it != entries.end());
    ASSERT_TRUE(it->second.is_string());
    ASSERT_TRUE(it->second.as_string()->value.find("hello_exec_test") != std::string::npos);
    auto it2 = entries.find("exit_code");
    ASSERT_TRUE(it2 != entries.end());
    ASSERT_EQ(it2->second.get_number(), 0.0);
}

// ===== Path Module Tests =====

TEST(path_join_basic) {
    akar::VM vm;
    auto r = vm.interpret("let v = path.join(\"/home\", \"user\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
    ASSERT_EQ(v.as_string()->value, "/home/user");
}

TEST(path_join_multiple) {
    akar::VM vm;
    auto r = vm.interpret("let v = path.join(\"/home\", \"user\", \"file.txt\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
    ASSERT_EQ(v.as_string()->value, "/home/user/file.txt");
}

TEST(path_dirname) {
    akar::VM vm;
    auto r = vm.interpret("let v = path.dirname(\"/home/user/file.txt\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
    ASSERT_EQ(v.as_string()->value, "/home/user");
}

TEST(path_basename) {
    akar::VM vm;
    auto r = vm.interpret("let v = path.basename(\"/home/user/file.txt\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
    ASSERT_EQ(v.as_string()->value, "file.txt");
}

TEST(path_ext) {
    akar::VM vm;
    auto r = vm.interpret("let v = path.ext(\"file.txt\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
    ASSERT_EQ(v.as_string()->value, ".txt");
}

TEST(path_stem) {
    akar::VM vm;
    auto r = vm.interpret("let v = path.stem(\"file.txt\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
    ASSERT_EQ(v.as_string()->value, "file");
}

// ===== IO Module Tests =====

TEST(io_write_and_read_file) {
    akar::VM vm;
    auto r = vm.interpret(
        "io.write_file(\"/tmp/akar_test_io.txt\", \"hello world\")\n"
        "let v = io.read_file(\"/tmp/akar_test_io.txt\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
    ASSERT_EQ(v.as_string()->value, "hello world");
}

TEST(io_append_file) {
    akar::VM vm;
    auto r = vm.interpret(
        "io.write_file(\"/tmp/akar_test_io2.txt\", \"line1\")\n"
        "io.append_file(\"/tmp/akar_test_io2.txt\", \"\\nline2\")\n"
        "let v = io.read_file(\"/tmp/akar_test_io2.txt\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
    ASSERT_EQ(v.as_string()->value, "line1\nline2");
}

TEST(io_lines_returns_array) {
    akar::VM vm;
    auto r = vm.interpret(
        "io.write_file(\"/tmp/akar_test_io3.txt\", \"a\\nb\\nc\\n\")\n"
        "let v = io.lines(\"/tmp/akar_test_io3.txt\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_array());
    ASSERT_EQ((int)v.as_array()->elements.size(), 3);
    ASSERT_EQ(v.as_array()->elements[0].as_string()->value, "a");
    ASSERT_EQ(v.as_array()->elements[1].as_string()->value, "b");
    ASSERT_EQ(v.as_array()->elements[2].as_string()->value, "c");
}

TEST(io_read_nonexistent_returns_nil) {
    akar::VM vm;
    auto r = vm.interpret("let v = io.read_file(\"/tmp/akar_nonexistent_999.txt\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_nil());
}

// ===== File Class Tests =====

TEST(file_class_open_read_close) {
    akar::VM vm;
    auto r = vm.interpret(
        "io.write_file(\"/tmp/akar_test_fc.txt\", \"hello file class\")\n"
        "let f = io.open(\"/tmp/akar_test_fc.txt\", \"r\")\n"
        "let v = f.read()\n"
        "f.close()");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
    ASSERT_EQ(v.as_string()->value, "hello file class");
}

TEST(file_class_readline) {
    akar::VM vm;
    auto r = vm.interpret(
        "io.write_file(\"/tmp/akar_test_fc2.txt\", \"line1\\nline2\\n\")\n"
        "let f = io.open(\"/tmp/akar_test_fc2.txt\", \"r\")\n"
        "let l1 = f.readline()\n"
        "let l2 = f.readline()\n"
        "f.close()");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto l1 = vm.get_global("l1");
    auto l2 = vm.get_global("l2");
    ASSERT_TRUE(l1.is_string());
    ASSERT_TRUE(l2.is_string());
    ASSERT_EQ(l1.as_string()->value, "line1\n");
    ASSERT_EQ(l2.as_string()->value, "line2\n");
}

TEST(file_class_write) {
    akar::VM vm;
    auto r = vm.interpret(
        "let f = File(\"/tmp/akar_test_fc3.txt\", \"w\")\n"
        "let n = f.write(\"written by File class\")\n"
        "f.close()\n"
        "let v = io.read_file(\"/tmp/akar_test_fc3.txt\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto n = vm.get_global("n");
    ASSERT_TRUE(n.is_number());
    ASSERT_EQ(n.get_number(), 21.0);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
    ASSERT_EQ(v.as_string()->value, "written by File class");
}

TEST(file_class_read_n_bytes) {
    akar::VM vm;
    auto r = vm.interpret(
        "io.write_file(\"/tmp/akar_test_fc4.txt\", \"abcdefghij\")\n"
        "let f = io.open(\"/tmp/akar_test_fc4.txt\", \"r\")\n"
        "let v = f.read(5)\n"
        "f.close()");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
    ASSERT_EQ(v.as_string()->value, "abcde");
}

TEST(file_class_tell_and_seek) {
    akar::VM vm;
    auto r = vm.interpret(
        "io.write_file(\"/tmp/akar_test_fc5.txt\", \"0123456789\")\n"
        "let f = io.open(\"/tmp/akar_test_fc5.txt\", \"r\")\n"
        "f.read(3)\n"
        "let pos = f.tell()\n"
        "f.seek(0)\n"
        "let pos2 = f.tell()\n"
        "f.close()");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto pos = vm.get_global("pos");
    auto pos2 = vm.get_global("pos2");
    ASSERT_TRUE(pos.is_number());
    ASSERT_EQ(pos.get_number(), 3.0);
    ASSERT_TRUE(pos2.is_number());
    ASSERT_EQ(pos2.get_number(), 0.0);
}

// ===== FS Module Tests =====

TEST(fs_exists_true) {
    akar::VM vm;
    auto r = vm.interpret("let v = fs.exists(\"/tmp\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_bool());
    ASSERT_TRUE(v.get_bool());
}

TEST(fs_exists_false) {
    akar::VM vm;
    auto r = vm.interpret("let v = fs.exists(\"/tmp/nonexistent_999\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_bool());
    ASSERT_TRUE(!v.get_bool());
}

TEST(fs_is_file_and_is_dir) {
    akar::VM vm;
    auto r = vm.interpret(
        "io.write_file(\"/tmp/akar_test_fs1.txt\", \"x\")\n"
        "let f = fs.is_file(\"/tmp/akar_test_fs1.txt\")\n"
        "let d = fs.is_dir(\"/tmp\")\n"
        "let nf = fs.is_file(\"/tmp\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    ASSERT_TRUE(vm.get_global("f").get_bool());
    ASSERT_TRUE(vm.get_global("d").get_bool());
    ASSERT_TRUE(!vm.get_global("nf").get_bool());
}

TEST(fs_mkdir_and_remove) {
    akar::VM vm;
    auto r = vm.interpret(
        "fs.mkdir(\"/tmp/akar_test_mkdir\")\n"
        "let ex = fs.exists(\"/tmp/akar_test_mkdir\")\n"
        "fs.remove(\"/tmp/akar_test_mkdir\")\n"
        "let ex2 = fs.exists(\"/tmp/akar_test_mkdir\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    ASSERT_TRUE(vm.get_global("ex").get_bool());
    ASSERT_TRUE(!vm.get_global("ex2").get_bool());
}

TEST(fs_readdir) {
    akar::VM vm;
    auto r = vm.interpret(
        "fs.mkdir(\"/tmp/akar_test_readdir\")\n"
        "io.write_file(\"/tmp/akar_test_readdir/a.txt\", \"x\")\n"
        "io.write_file(\"/tmp/akar_test_readdir/b.txt\", \"x\")\n"
        "let v = fs.readdir(\"/tmp/akar_test_readdir\")\n"
        "fs.remove(\"/tmp/akar_test_readdir\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_array());
    ASSERT_EQ((int)v.as_array()->elements.size(), 2);
}

TEST(fs_rename) {
    akar::VM vm;
    auto r = vm.interpret(
        "io.write_file(\"/tmp/akar_test_rename_old.txt\", \"data\")\n"
        "fs.rename(\"/tmp/akar_test_rename_old.txt\", \"/tmp/akar_test_rename_new.txt\")\n"
        "let v = io.read_file(\"/tmp/akar_test_rename_new.txt\")\n"
        "let gone = fs.exists(\"/tmp/akar_test_rename_old.txt\")\n"
        "fs.remove(\"/tmp/akar_test_rename_new.txt\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
    ASSERT_EQ(v.as_string()->value, "data");
    ASSERT_TRUE(!vm.get_global("gone").get_bool());
}

TEST(fs_copy) {
    akar::VM vm;
    auto r = vm.interpret(
        "io.write_file(\"/tmp/akar_test_copy_src.txt\", \"copied\")\n"
        "fs.copy(\"/tmp/akar_test_copy_src.txt\", \"/tmp/akar_test_copy_dst.txt\")\n"
        "let v = io.read_file(\"/tmp/akar_test_copy_dst.txt\")\n"
        "fs.remove(\"/tmp/akar_test_copy_src.txt\")\n"
        "fs.remove(\"/tmp/akar_test_copy_dst.txt\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
    ASSERT_EQ(v.as_string()->value, "copied");
}

TEST(fs_size) {
    akar::VM vm;
    auto r = vm.interpret(
        "io.write_file(\"/tmp/akar_test_size.txt\", \"12345\")\n"
        "let v = fs.size(\"/tmp/akar_test_size.txt\")\n"
        "fs.remove(\"/tmp/akar_test_size.txt\")");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_number());
    ASSERT_EQ(v.get_number(), 5.0);
}

TEST(fs_cwd_returns_string) {
    akar::VM vm;
    auto r = vm.interpret("let v = fs.cwd()");
    ASSERT_TRUE(r == akar::InterpretResult::Ok);
    auto v = vm.get_global("v");
    ASSERT_TRUE(v.is_string());
    ASSERT_TRUE(v.as_string()->value.size() > 0);
}

// ===== Net Module Tests (TCP loopback) =====

TEST(net_tcp_loopback) {
    akar::VM vm;
    // Start a TCP server on a random high port
    vm.interpret("let srv = net.listen(\"127.0.0.1\", 0)");
    // We can't easily test TCP in a single interpret due to blocking.
    // Just test that server creation and connection basics work.
    // The listen with port 0 should succeed.
    auto srv = vm.get_global("srv");
    ASSERT_TRUE(srv.is_instance());
    ASSERT_EQ(srv.as_instance()->klass->name, "TcpServer");
    // Close server
    vm.interpret("srv.close()");
}

// ===== Cleanup =====
static void cleanup_test_files() {
    // Clean up any leftover test files
    const char* files[] = {
        "/tmp/akar_test_io.txt", "/tmp/akar_test_io2.txt", "/tmp/akar_test_io3.txt",
        "/tmp/akar_test_fc.txt", "/tmp/akar_test_fc2.txt", "/tmp/akar_test_fc3.txt",
        "/tmp/akar_test_fc4.txt", "/tmp/akar_test_fc5.txt",
        "/tmp/akar_test_fs1.txt",
        nullptr
    };
    for (int i = 0; files[i]; i++) unlink(files[i]);
}
