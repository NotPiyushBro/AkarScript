#include "akar/vm/system.h"
#include "akar/vm/vm.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <ctime>

// POSIX headers
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

// Native library extensions
#include "akar/native/ssl.h"

// TCP / sockets
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// System info
#include <sys/utsname.h>

namespace fs = std::filesystem;

namespace akar {

// ---------------------------------------------------------------------------
// Internal handle tables (process-global, not tied to any single VM)
// ---------------------------------------------------------------------------

static std::unordered_map<int, FILE*> g_open_files;
static int g_next_file_handle = 1;

static std::unordered_map<int, int> g_open_sockets;  // handle -> fd
static int g_next_sock_handle = 1;

static int safe_int(double d) {
    if (d != d) return 0;
    if (d > 2147483647.0) return 2147483647;
    if (d < -2147483648.0) return -2147483648;
    return static_cast<int>(d);
}

// ---------------------------------------------------------------------------
// Helper: create a module map (ObjMap filled with native functions)
// ---------------------------------------------------------------------------

static ObjMap* make_module() { return allocate_map(); }

// Register a native function inside a module map.
// IMPORTANT: In Akar, `map.fn(args)` is a method call, so the compiler
// passes the map itself as argv[0]. We wrap every function to strip that
// leading "self" argument, so the inner function sees only user-supplied args.
static void mod_put(ObjMap* m, const char* name, NativeFn fn) {
    // Capture fn by move into the wrapper
    auto wrapped = [f = std::move(fn)](int argc, Value* argv) -> Value {
        // argv[0] is the module map (self); real args start at argv[1]
        if (argc >= 1) { argc--; argv++; }
        return f(argc, argv);
    };
    m->entries[name] = Value(static_cast<Obj*>(allocate_native(std::move(wrapped), name)));
}

// ---------------------------------------------------------------------------
// File class – stores a handle number in instance fields["_handle"]
// ---------------------------------------------------------------------------

static int get_handle(Value inst) {
    if (!inst.is_instance()) return -1;
    auto it = inst.as_instance()->fields.find("_handle");
    if (it == inst.as_instance()->fields.end()) return -1;
    return safe_int(it->second.get_number());
}

static FILE* get_file(Value inst) {
    int h = get_handle(inst);
    if (h < 0) return nullptr;
    auto it = g_open_files.find(h);
    return (it != g_open_files.end()) ? it->second : nullptr;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Embedded Akar HTTP library — written in .ak, auto-loaded at VM init
//   Uses: substr, split, contains, replace (built-in natives)
//   Implements: str_find, starts_with, trim (helpers)
// ---------------------------------------------------------------------------

static const char* http_ak_source = R"akar(fn str_find(haystack, needle) {
    let hlen = len(haystack)
    let nlen = len(needle)
    if (nlen == 0) { return 0 }
    let i = 0
    while (i <= hlen - nlen) {
        if (substr(haystack, i, nlen) == needle) { return i }
        i = i + 1
    }
    return -1
}
fn starts_with(str, prefix) {
    if (len(str) < len(prefix)) { return false }
    return substr(str, 0, len(prefix)) == prefix
}
fn trim(str) {
    let start = 0
    let slen = len(str)
    while (start < slen) {
        let c = str[start]
        if (c != " " and c != "\t" and c != "\r" and c != "\n") { break }
        start = start + 1
    }
    let end = slen - 1
    while (end >= start) {
        let c = str[end]
        if (c != " " and c != "\t" and c != "\r" and c != "\n") { break }
        end = end - 1
    }
    if (end < start) { return "" }
    return substr(str, start, end - start + 1)
}
fn hex_val(c) {
    if (c == "0") { return 0 }
    if (c == "1") { return 1 }
    if (c == "2") { return 2 }
    if (c == "3") { return 3 }
    if (c == "4") { return 4 }
    if (c == "5") { return 5 }
    if (c == "6") { return 6 }
    if (c == "7") { return 7 }
    if (c == "8") { return 8 }
    if (c == "9") { return 9 }
    if (c == "a" or c == "A") { return 10 }
    if (c == "b" or c == "B") { return 11 }
    if (c == "c" or c == "C") { return 12 }
    if (c == "d" or c == "D") { return 13 }
    if (c == "e" or c == "E") { return 14 }
    if (c == "f" or c == "F") { return 15 }
    return -1
}
fn parse_hex(s) {
    let result = 0
    let i = 0
    s = trim(s)
    while (i < len(s)) {
        let d = hex_val(s[i])
        if (d < 0) { break }
        result = result * 16 + d
        i = i + 1
    }
    return result
}
fn decode_chunked(data) {
    let result = ""
    let pos = 0
    let dlen = len(data)
    while (pos < dlen) {
        let remaining = substr(data, pos, dlen - pos)
        let line_end = str_find(remaining, "\r\n")
        let nl_len = 2
        if (line_end < 0) {
            line_end = str_find(remaining, "\n")
            nl_len = 1
        }
        if (line_end < 0) { break }
        let chunk_size = parse_hex(substr(data, pos, line_end))
        if (chunk_size <= 0) { break }
        pos = pos + line_end + nl_len
        if (pos + chunk_size <= dlen) {
            result = result + substr(data, pos, chunk_size)
        }
        pos = pos + chunk_size
        if (pos + 2 <= dlen and substr(data, pos, 2) == "\r\n") {
            pos = pos + 2
        } else if (pos < dlen and substr(data, pos, 1) == "\n") {
            pos = pos + 1
        }
    }
    return result
}
fn parse_url(url) {
    let scheme = "http"
    let host = ""
    let port = 80
    let path = "/"
    if (starts_with(url, "https://")) {
        scheme = "https"
        port = 443
        url = substr(url, 8, len(url) - 8)
    } else if (starts_with(url, "http://")) {
        url = substr(url, 7, len(url) - 7)
    }
    let slash = str_find(url, "/")
    let host_port = ""
    if (slash >= 0) {
        host_port = substr(url, 0, slash)
        path = substr(url, slash, len(url) - slash)
    } else {
        host_port = url
    }
    let colon = str_find(host_port, ":")
    if (colon >= 0) {
        host = substr(host_port, 0, colon)
        let port_str = substr(host_port, colon + 1, len(host_port) - colon - 1)
        let p = to_number(port_str)
        if (p != nil and p > 0) { port = p }
    } else {
        host = host_port
    }
    return {"scheme": scheme, "host": host, "port": port, "path": path}
}
fn parse_response(raw) {
    let resp = {"status": 0, "status_text": "", "headers": {}, "body": ""}
    let sep = str_find(raw, "\r\n\r\n")
    let hdr_part = ""
    if (sep >= 0) {
        hdr_part = substr(raw, 0, sep)
        resp.body = substr(raw, sep + 4, len(raw) - sep - 4)
    } else {
        sep = str_find(raw, "\n\n")
        if (sep >= 0) {
            hdr_part = substr(raw, 0, sep)
            resp.body = substr(raw, sep + 2, len(raw) - sep - 2)
        } else {
            resp.body = raw
            return resp
        }
    }
    let lines = split(hdr_part, "\r\n")
    if (len(lines) == 0) { lines = split(hdr_part, "\n") }
    if (len(lines) > 0) {
        let parts = split(lines[0], " ")
        if (len(parts) >= 2) {
            let s = to_number(parts[1])
            if (s != nil) { resp.status = s }
        }
        if (len(parts) >= 3) {
            resp.status_text = parts[2]
            let j = 3
            while (j < len(parts)) {
                resp.status_text = resp.status_text + " " + parts[j]
                j = j + 1
            }
        }
    }
    let i = 1
    while (i < len(lines)) {
        let line = lines[i]
        let cp = str_find(line, ":")
        if (cp > 0) {
            let key = trim(substr(line, 0, cp))
            let val = trim(substr(line, cp + 1, len(line) - cp - 1))
            resp.headers[key] = val
        }
        i = i + 1
    }
    let te = ""
    if (resp.headers["Transfer-Encoding"] != nil) { te = resp.headers["Transfer-Encoding"] }
    if (contains(te, "chunked")) { resp.body = decode_chunked(resp.body) }
    return resp
}
fn http_request(method, url, body, headers) {
    let u = parse_url(url)
    let use_ssl = (u.scheme == "https")
    let sock = nil
    if (use_ssl) {
        sock = net.ssl_connect(u.host, u.port)
    } else {
        sock = net.connect(u.host, u.port)
    }
    if (sock == nil) { return nil }
    let req = method + " " + u.path + " HTTP/1.1\r\n"
    req = req + "Host: " + u.host + "\r\n"
    req = req + "Connection: close\r\n"
    req = req + "User-Agent: AkarScript/1.0\r\n"
    let body_str = ""
    if (body != nil) { body_str = to_string(body) }
    if (len(body_str) > 0) {
        req = req + "Content-Length: " + to_string(len(body_str)) + "\r\n"
    }
    if (headers != nil) {
        let hdrs = keys(headers)
        let hi = 0
        while (hi < len(hdrs)) {
            let key = hdrs[hi]
            req = req + key + ": " + to_string(headers[key]) + "\r\n"
            hi = hi + 1
        }
    }
    req = req + "\r\n"
    if (len(body_str) > 0) { req = req + body_str }
    sock.send(req)
    let raw = ""
    let empties = 0
    while (empties < 3) {
        let chunk = sock.recv(4096, 3000)
        if (chunk == nil or len(chunk) == 0) {
            empties = empties + 1
        } else {
            raw = raw + chunk
            empties = 0
        }
    }
    sock.close()
    if (len(raw) == 0) { return nil }
    return parse_response(raw)
}
fn http_get(url, headers) { return http_request("GET", url, nil, headers) }
fn http_post(url, body, headers) { return http_request("POST", url, body, headers) }
fn http_put(url, body, headers) { return http_request("PUT", url, body, headers) }
fn http_patch(url, body, headers) { return http_request("PATCH", url, body, headers) }
fn http_delete(url, headers) { return http_request("DELETE", url, nil, headers) }
fn http_head(url, headers) { return http_request("HEAD", url, nil, headers) }
)akar";

void register_system_libs(VM& vm) {

    // ========================================================================
    // File class
    // ========================================================================
    auto* file_cls = allocate_class("File");

    // init(path, mode) – open a file
    // NOTE: For class methods, argv[0] = this (the instance), argv[1..] = user args
    file_cls->methods["init"] = Value(static_cast<Obj*>(allocate_native(
        [](int argc, Value* argv) -> Value {
            if (argc < 3 || !argv[1].is_string() || !argv[2].is_string()) return Value();
            std::string path = argv[1].as_string()->value;
            std::string mode = argv[2].as_string()->value;
            FILE* f = fopen(path.c_str(), mode.c_str());
            if (!f) return Value();
            int h = g_next_file_handle++;
            g_open_files[h] = f;
            argv[0].as_instance()->fields["_handle"] = Value(static_cast<double>(h));
            return argv[0]; // return this
        }, "File.init")));

    // read(n?) → string
    file_cls->methods["read"] = Value(static_cast<Obj*>(allocate_native(
        [](int argc, Value* argv) -> Value {
            FILE* f = get_file(argv[0]);
            if (!f) return Value();
            if (argc >= 2 && argv[1].is_number()) {
                int n = safe_int(argv[1].get_number());
                if (n <= 0) return Value(static_cast<Obj*>(get_string_table().intern("")));
                std::string buf(n, '\0');
                size_t got = fread(&buf[0], 1, n, f);
                buf.resize(got);
                return Value(static_cast<Obj*>(get_string_table().intern(buf)));
            }
            // read all
            std::ostringstream oss;
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), f)) > 0) oss.write(buf, n);
            return Value(static_cast<Obj*>(get_string_table().intern(oss.str())));
        }, "File.read")));

    // readline() → string or nil
    file_cls->methods["readline"] = Value(static_cast<Obj*>(allocate_native(
        [](int argc, Value* argv) -> Value {
            FILE* f = get_file(argv[0]);
            if (!f) return Value();
            std::string line;
            int c;
            while ((c = fgetc(f)) != EOF) {
                line += static_cast<char>(c);
                if (c == '\n') break;
            }
            if (line.empty() && feof(f)) return Value();
            return Value(static_cast<Obj*>(get_string_table().intern(line)));
        }, "File.readline")));

    // readlines() → array
    file_cls->methods["readlines"] = Value(static_cast<Obj*>(allocate_native(
        [](int argc, Value* argv) -> Value {
            FILE* f = get_file(argv[0]);
            if (!f) return Value();
            auto* arr = allocate_array();
            std::string line;
            int c;
            while ((c = fgetc(f)) != EOF) {
                line += static_cast<char>(c);
                if (c == '\n') {
                    arr->elements.push_back(Value(static_cast<Obj*>(get_string_table().intern(line))));
                    line.clear();
                }
            }
            if (!line.empty()) {
                arr->elements.push_back(Value(static_cast<Obj*>(get_string_table().intern(line))));
            }
            return Value(static_cast<Obj*>(arr));
        }, "File.readlines")));

    // write(data) → number of bytes written
    file_cls->methods["write"] = Value(static_cast<Obj*>(allocate_native(
        [](int argc, Value* argv) -> Value {
            FILE* f = get_file(argv[0]);
            if (!f || argc < 2 || !argv[1].is_string()) return Value(0.0);
            auto& s = argv[1].as_string()->value;
            size_t written = fwrite(s.data(), 1, s.size(), f);
            fflush(f);
            return Value(static_cast<double>(written));
        }, "File.write")));

    // tell() → position
    file_cls->methods["tell"] = Value(static_cast<Obj*>(allocate_native(
        [](int argc, Value* argv) -> Value {
            FILE* f = get_file(argv[0]);
            if (!f) return Value(-1.0);
            long pos = ftell(f);
            return Value(static_cast<double>(pos));
        }, "File.tell")));

    // seek(offset, whence?) → bool  (whence: 0=set, 1=cur, 2=end)
    file_cls->methods["seek"] = Value(static_cast<Obj*>(allocate_native(
        [](int argc, Value* argv) -> Value {
            FILE* f = get_file(argv[0]);
            if (!f || argc < 2 || !argv[1].is_number()) return Value(false);
            long offset = static_cast<long>(argv[1].get_number());
            int whence = SEEK_SET;
            if (argc >= 3 && argv[2].is_number()) {
                int w = safe_int(argv[2].get_number());
                if (w == 1) whence = SEEK_CUR;
                else if (w == 2) whence = SEEK_END;
            }
            return Value(fseek(f, offset, whence) == 0);
        }, "File.seek")));

    // eof() → bool
    file_cls->methods["eof"] = Value(static_cast<Obj*>(allocate_native(
        [](int argc, Value* argv) -> Value {
            FILE* f = get_file(argv[0]);
            if (!f) return Value(true);
            return Value(feof(f) != 0);
        }, "File.eof")));

    // flush() → nil
    file_cls->methods["flush"] = Value(static_cast<Obj*>(allocate_native(
        [](int argc, Value* argv) -> Value {
            FILE* f = get_file(argv[0]);
            if (f) fflush(f);
            return Value();
        }, "File.flush")));

    // close() → nil
    file_cls->methods["close"] = Value(static_cast<Obj*>(allocate_native(
        [](int argc, Value* argv) -> Value {
            int h = get_handle(argv[0]);
            if (h < 0) return Value();
            auto it = g_open_files.find(h);
            if (it != g_open_files.end()) {
                fclose(it->second);
                g_open_files.erase(it);
            }
            argv[0].as_instance()->fields.erase("_handle");
            return Value();
        }, "File.close")));

    vm.set_global("File", Value(static_cast<Obj*>(file_cls)));

    // ========================================================================
    // io module – file I/O convenience functions
    // ========================================================================
    auto* io = make_module();

    // io.open(path, mode) → File instance
    mod_put(io, "open", [file_cls](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        std::string path = argv[0].as_string()->value;
        std::string mode = (argc >= 2 && argv[1].is_string()) ? argv[1].as_string()->value : "r";
        FILE* f = fopen(path.c_str(), mode.c_str());
        if (!f) return Value();
        int h = g_next_file_handle++;
        g_open_files[h] = f;
        auto* inst = allocate_instance(file_cls);
        inst->fields["_handle"] = Value(static_cast<double>(h));
        return Value(static_cast<Obj*>(inst));
    });

    // io.read_file(path) → string or nil
    mod_put(io, "read_file", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        std::ifstream ifs(argv[0].as_string()->value);
        if (!ifs.is_open()) return Value();
        std::ostringstream oss;
        oss << ifs.rdbuf();
        return Value(static_cast<Obj*>(get_string_table().intern(oss.str())));
    });

    // io.write_file(path, content) → bool
    mod_put(io, "write_file", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_string() || !argv[1].is_string()) return Value(false);
        std::ofstream ofs(argv[0].as_string()->value, std::ios::trunc);
        if (!ofs.is_open()) return Value(false);
        ofs << argv[1].as_string()->value;
        return Value(static_cast<bool>(ofs.good()));
    });

    // io.append_file(path, content) → bool
    mod_put(io, "append_file", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_string() || !argv[1].is_string()) return Value(false);
        std::ofstream ofs(argv[0].as_string()->value, std::ios::app);
        if (!ofs.is_open()) return Value(false);
        ofs << argv[1].as_string()->value;
        return Value(static_cast<bool>(ofs.good()));
    });

    // io.lines(path) → array of strings or nil
    mod_put(io, "lines", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        std::ifstream ifs(argv[0].as_string()->value);
        if (!ifs.is_open()) return Value();
        auto* arr = allocate_array();
        std::string line;
        while (std::getline(ifs, line)) {
            arr->elements.push_back(Value(static_cast<Obj*>(get_string_table().intern(line))));
        }
        return Value(static_cast<Obj*>(arr));
    });

    vm.set_global("io", Value(static_cast<Obj*>(io)));

    // ========================================================================
    // fs module – filesystem operations
    // ========================================================================
    auto* fs_mod = make_module();

    mod_put(fs_mod, "exists", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value(false);
        return Value(fs::exists(argv[0].as_string()->value));
    });

    mod_put(fs_mod, "is_file", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value(false);
        std::error_code ec;
        return Value(fs::is_regular_file(argv[0].as_string()->value, ec));
    });

    mod_put(fs_mod, "is_dir", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value(false);
        std::error_code ec;
        return Value(fs::is_directory(argv[0].as_string()->value, ec));
    });

    mod_put(fs_mod, "size", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        std::error_code ec;
        auto sz = fs::file_size(argv[0].as_string()->value, ec);
        if (ec) return Value();
        return Value(static_cast<double>(sz));
    });

    mod_put(fs_mod, "readdir", [](int argc, Value* argv) -> Value {
        std::string path = (argc >= 1 && argv[0].is_string()) ? argv[0].as_string()->value : ".";
        std::error_code ec;
        auto* arr = allocate_array();
        for (auto& entry : fs::directory_iterator(path, ec)) {
            arr->elements.push_back(Value(static_cast<Obj*>(
                get_string_table().intern(entry.path().filename().string()))));
        }
        if (ec) return Value();
        return Value(static_cast<Obj*>(arr));
    });

    mod_put(fs_mod, "mkdir", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value(false);
        bool recursive = (argc >= 2 && argv[1].is_truthy());
        std::error_code ec;
        if (recursive) fs::create_directories(argv[0].as_string()->value, ec);
        else fs::create_directory(argv[0].as_string()->value, ec);
        return Value(!ec);
    });

    mod_put(fs_mod, "remove", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value(false);
        std::error_code ec;
        fs::remove_all(argv[0].as_string()->value, ec);
        return Value(!ec);
    });

    mod_put(fs_mod, "rename", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_string() || !argv[1].is_string()) return Value(false);
        std::error_code ec;
        fs::rename(argv[0].as_string()->value, argv[1].as_string()->value, ec);
        return Value(!ec);
    });

    mod_put(fs_mod, "copy", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_string() || !argv[1].is_string()) return Value(false);
        std::error_code ec;
        fs::copy_file(argv[0].as_string()->value, argv[1].as_string()->value,
                      fs::copy_options::overwrite_existing, ec);
        return Value(!ec);
    });

    mod_put(fs_mod, "cwd", [](int, Value*) -> Value {
        std::error_code ec;
        auto p = fs::current_path(ec);
        if (ec) return Value();
        return Value(static_cast<Obj*>(get_string_table().intern(p.string())));
    });

    mod_put(fs_mod, "chdir", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value(false);
        std::error_code ec;
        fs::current_path(argv[0].as_string()->value, ec);
        return Value(!ec);
    });

    vm.set_global("fs", Value(static_cast<Obj*>(fs_mod)));

    // ========================================================================
    // path module – path manipulation
    // ========================================================================
    auto* path_mod = make_module();

    mod_put(path_mod, "join", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        fs::path p(argv[0].as_string()->value);
        for (int i = 1; i < argc; i++) {
            if (argv[i].is_string()) p /= argv[i].as_string()->value;
        }
        return Value(static_cast<Obj*>(get_string_table().intern(p.string())));
    });

    mod_put(path_mod, "dirname", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        return Value(static_cast<Obj*>(get_string_table().intern(
            fs::path(argv[0].as_string()->value).parent_path().string())));
    });

    mod_put(path_mod, "basename", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        return Value(static_cast<Obj*>(get_string_table().intern(
            fs::path(argv[0].as_string()->value).filename().string())));
    });

    mod_put(path_mod, "ext", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        return Value(static_cast<Obj*>(get_string_table().intern(
            fs::path(argv[0].as_string()->value).extension().string())));
    });

    mod_put(path_mod, "abs", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        std::error_code ec;
        auto p = fs::absolute(argv[0].as_string()->value, ec);
        if (ec) return Value();
        return Value(static_cast<Obj*>(get_string_table().intern(p.string())));
    });

    mod_put(path_mod, "stem", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        return Value(static_cast<Obj*>(get_string_table().intern(
            fs::path(argv[0].as_string()->value).stem().string())));
    });

    vm.set_global("path", Value(static_cast<Obj*>(path_mod)));

    // ========================================================================
    // sys module – environment, process, OS info
    // ========================================================================
    auto* sys = make_module();

    mod_put(sys, "env", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        const char* val = getenv(argv[0].as_string()->value.c_str());
        if (!val) return Value();
        return Value(static_cast<Obj*>(get_string_table().intern(val)));
    });

    mod_put(sys, "set_env", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_string() || !argv[1].is_string()) return Value(false);
        int rc = setenv(argv[0].as_string()->value.c_str(), argv[1].as_string()->value.c_str(), 1);
        return Value(rc == 0);
    });

    mod_put(sys, "env_all", [](int, Value*) -> Value {
        auto* m = allocate_map();
        for (char** env = environ; *env; ++env) {
            std::string entry(*env);
            auto eq = entry.find('=');
            if (eq != std::string::npos) {
                m->entries[entry.substr(0, eq)] = Value(static_cast<Obj*>(
                    get_string_table().intern(entry.substr(eq + 1))));
            }
        }
        return Value(static_cast<Obj*>(m));
    });

    mod_put(sys, "os", [](int, Value*) -> Value {
        struct utsname u;
        if (uname(&u) != 0) return Value(static_cast<Obj*>(get_string_table().intern("unknown")));
        std::string s(u.sysname);
        for (auto& c : s) c = static_cast<char>(std::tolower(c));
        return Value(static_cast<Obj*>(get_string_table().intern(s)));
    });

    mod_put(sys, "arch", [](int, Value*) -> Value {
        struct utsname u;
        if (uname(&u) != 0) return Value(static_cast<Obj*>(get_string_table().intern("unknown")));
        return Value(static_cast<Obj*>(get_string_table().intern(u.machine)));
    });

    mod_put(sys, "hostname", [](int, Value*) -> Value {
        struct utsname u;
        if (uname(&u) != 0) return Value(static_cast<Obj*>(get_string_table().intern("unknown")));
        return Value(static_cast<Obj*>(get_string_table().intern(u.nodename)));
    });

    mod_put(sys, "cpu_count", [](int, Value*) -> Value {
        long n = sysconf(_SC_NPROCESSORS_ONLN);
        return Value(static_cast<double>(n > 0 ? n : 1));
    });

    mod_put(sys, "pid", [](int, Value*) -> Value {
        return Value(static_cast<double>(getpid()));
    });

    mod_put(sys, "exec", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        std::string cmd = argv[0].as_string()->value + " 2>&1";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return Value();
        auto* m = allocate_map();
        std::ostringstream oss;
        char buf[4096];
        while (fgets(buf, sizeof(buf), pipe)) oss << buf;
        int rc = pclose(pipe);
        m->entries["stdout"] = Value(static_cast<Obj*>(get_string_table().intern(oss.str())));
        m->entries["stderr"] = Value(static_cast<Obj*>(get_string_table().intern(std::string())));
        m->entries["exit_code"] = Value(static_cast<double>(WEXITSTATUS(rc)));
        return Value(static_cast<Obj*>(m));
    });

    mod_put(sys, "exec2", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_string()) return Value();
        std::string cmd = argv[0].as_string()->value;
        std::string tmp = "/tmp/akar_exec_" + std::to_string(getpid()) + "_" + std::to_string(clock());
        std::string full_cmd = cmd + " >" + tmp + ".out 2>" + tmp + ".err";
        int rc = system(full_cmd.c_str());
        auto slurp = [&](const std::string& p) -> std::string {
            std::ifstream ifs(p);
            if (!ifs.is_open()) return std::string();
            std::ostringstream o; o << ifs.rdbuf(); return o.str();
        };
        auto* m = allocate_map();
        m->entries["stdout"] = Value(static_cast<Obj*>(get_string_table().intern(slurp(tmp + ".out"))));
        m->entries["stderr"] = Value(static_cast<Obj*>(get_string_table().intern(slurp(tmp + ".err"))));
        m->entries["exit_code"] = Value(static_cast<double>(WEXITSTATUS(rc)));
        unlink((tmp + ".out").c_str());
        unlink((tmp + ".err").c_str());
        return Value(static_cast<Obj*>(m));
    });

    mod_put(sys, "now", [](int, Value*) -> Value {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        struct tm tm_buf;
        localtime_r(&t, &tm_buf);
        char buf[64];
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_buf);
        return Value(static_cast<Obj*>(get_string_table().intern(buf)));
    });

    mod_put(sys, "sleep", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].is_number()) return Value();
        int ms = safe_int(argv[0].get_number());
        if (ms > 0) usleep(static_cast<useconds_t>(ms) * 1000);
        return Value();
    });

    mod_put(sys, "exit", [](int argc, Value* argv) -> Value {
        int code = 0;
        if (argc >= 1 && argv[0].is_number()) code = safe_int(argv[0].get_number());
        std::exit(code);
        return Value();
    });

    vm.set_global("sys", Value(static_cast<Obj*>(sys)));

    // ========================================================================
    // net module – TCP networking
    // ========================================================================
    auto* net = make_module();

    // TcpSocket class
    auto* tcp_sock_cls = allocate_class("TcpSocket");

    tcp_sock_cls->methods["send"] = Value(static_cast<Obj*>(allocate_native(
        [](int argc, Value* argv) -> Value {
            int h = get_handle(argv[0]);
            auto it = g_open_sockets.find(h);
            if (it == g_open_sockets.end() || argc < 2 || !argv[1].is_string()) return Value(0.0);
            auto& data = argv[1].as_string()->value;
            ssize_t sent = ::send(it->second, data.data(), data.size(), MSG_NOSIGNAL);
            return Value(static_cast<double>(sent > 0 ? sent : 0));
        }, "TcpSocket.send")));

    tcp_sock_cls->methods["recv"] = Value(static_cast<Obj*>(allocate_native(
        [](int argc, Value* argv) -> Value {
            int h = get_handle(argv[0]);
            auto it = g_open_sockets.find(h);
            if (it == g_open_sockets.end()) return Value();
            int n = (argc >= 2 && argv[1].is_number()) ? safe_int(argv[1].get_number()) : 4096;
            if (n <= 0) n = 4096;
            std::string buf(n, '\0');
            ssize_t got = recv(it->second, &buf[0], n, 0);
            if (got <= 0) return Value(static_cast<Obj*>(get_string_table().intern("")));
            buf.resize(static_cast<size_t>(got));
            return Value(static_cast<Obj*>(get_string_table().intern(buf)));
        }, "TcpSocket.recv")));

    tcp_sock_cls->methods["close"] = Value(static_cast<Obj*>(allocate_native(
        [](int argc, Value* argv) -> Value {
            int h = get_handle(argv[0]);
            auto it = g_open_sockets.find(h);
            if (it != g_open_sockets.end()) {
                ::close(it->second);
                g_open_sockets.erase(it);
            }
            argv[0].as_instance()->fields.erase("_handle");
            return Value();
        }, "TcpSocket.close")));

    tcp_sock_cls->methods["peer"] = Value(static_cast<Obj*>(allocate_native(
        [](int argc, Value* argv) -> Value {
            int h = get_handle(argv[0]);
            auto it = g_open_sockets.find(h);
            if (it == g_open_sockets.end()) return Value();
            struct sockaddr_in addr{};
            socklen_t len = sizeof(addr);
            getpeername(it->second, reinterpret_cast<struct sockaddr*>(&addr), &len);
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
            std::string result = std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
            return Value(static_cast<Obj*>(get_string_table().intern(result)));
        }, "TcpSocket.peer")));

    vm.set_global("TcpSocket", Value(static_cast<Obj*>(tcp_sock_cls)));

    // TcpServer class
    auto* tcp_srv_cls = allocate_class("TcpServer");

    tcp_srv_cls->methods["accept"] = Value(static_cast<Obj*>(allocate_native(
        [tcp_sock_cls](int argc, Value* argv) -> Value {
            int h = get_handle(argv[0]);
            auto it = g_open_sockets.find(h);
            if (it == g_open_sockets.end()) return Value();
            struct sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_fd = ::accept(it->second, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
            if (client_fd < 0) return Value();
            int ch = g_next_sock_handle++;
            g_open_sockets[ch] = client_fd;
            auto* inst = allocate_instance(tcp_sock_cls);
            inst->fields["_handle"] = Value(static_cast<double>(ch));
            return Value(static_cast<Obj*>(inst));
        }, "TcpServer.accept")));

    tcp_srv_cls->methods["close"] = Value(static_cast<Obj*>(allocate_native(
        [](int argc, Value* argv) -> Value {
            int h = get_handle(argv[0]);
            auto it = g_open_sockets.find(h);
            if (it != g_open_sockets.end()) {
                ::close(it->second);
                g_open_sockets.erase(it);
            }
            argv[0].as_instance()->fields.erase("_handle");
            return Value();
        }, "TcpServer.close")));

    tcp_srv_cls->methods["addr"] = Value(static_cast<Obj*>(allocate_native(
        [](int argc, Value* argv) -> Value {
            int h = get_handle(argv[0]);
            auto it = g_open_sockets.find(h);
            if (it == g_open_sockets.end()) return Value();
            struct sockaddr_in addr{};
            socklen_t len = sizeof(addr);
            getsockname(it->second, reinterpret_cast<struct sockaddr*>(&addr), &len);
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
            std::string result = std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
            return Value(static_cast<Obj*>(get_string_table().intern(result)));
        }, "TcpServer.addr")));

    vm.set_global("TcpServer", Value(static_cast<Obj*>(tcp_srv_cls)));

    // Now that we have the real class pointers, register net.connect and net.listen
    // with proper class captures
    mod_put(net, "connect", [tcp_sock_cls](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].is_string() || !argv[1].is_number()) return Value();
        std::string host = argv[0].as_string()->value;
        int port = safe_int(argv[1].get_number());

        struct addrinfo hints{}, *res;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        std::string port_str = std::to_string(port);
        if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0) return Value();

        int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd < 0) { freeaddrinfo(res); return Value(); }

        if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
            ::close(fd);
            freeaddrinfo(res);
            return Value();
        }
        freeaddrinfo(res);

        int h = g_next_sock_handle++;
        g_open_sockets[h] = fd;
        auto* inst = allocate_instance(tcp_sock_cls);
        inst->fields["_handle"] = Value(static_cast<double>(h));
        return Value(static_cast<Obj*>(inst));
    });

    mod_put(net, "listen", [tcp_srv_cls](int argc, Value* argv) -> Value {
        std::string host = "0.0.0.0";
        int port = 0;
        if (argc >= 2 && argv[0].is_string() && argv[1].is_number()) {
            host = argv[0].as_string()->value;
            port = safe_int(argv[1].get_number());
        } else if (argc >= 1 && argv[0].is_number()) {
            port = safe_int(argv[0].get_number());
        } else {
            return Value();
        }

        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return Value();

        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr_s{};
        addr_s.sin_family = AF_INET;
        addr_s.sin_port = htons(static_cast<uint16_t>(port));
        inet_pton(AF_INET, host.c_str(), &addr_s.sin_addr);

        if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr_s), sizeof(addr_s)) < 0) {
            ::close(fd);
            return Value();
        }
        if (listen(fd, 128) < 0) {
            ::close(fd);
            return Value();
        }

        int h = g_next_sock_handle++;
        g_open_sockets[h] = fd;
        auto* inst = allocate_instance(tcp_srv_cls);
        inst->fields["_handle"] = Value(static_cast<double>(h));
        return Value(static_cast<Obj*>(inst));
    });

    vm.set_global("net", Value(static_cast<Obj*>(net)));

    // Register native library extensions
    register_ssl_native(vm, net);

    // Load the Akar HTTP/HTTPS library (written in .ak, embedded above)
    vm.interpret(http_ak_source);
}

} // namespace akar
