#include "akar/vm/vm.h"
#include "akar/vm/object_file.h"
#include "akar/compiler/lexer.h"
#include "akar/compiler/parser.h"
#include "akar/compiler/codegen.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include "akar/util/repl.h"
#include <string>

static bool is_incomplete(const std::string& code) {
    int depth = 0;
    bool in_str = false;
    char sc = 0;
    for (size_t i = 0; i < code.size(); i++) {
        char c = code[i];
        if (in_str) {
            if (c == sc && (i == 0 || code[i - 1] != '\\')) in_str = false;
            continue;
        }
        if (c == '"' || c == '\'') { in_str = true; sc = c; continue; }
        if (c == '/' && i + 1 < code.size() && code[i + 1] == '/') {
            while (i < code.size() && code[i] != '\n') i++;
            continue;
        }
        if (c == '{' || c == '(' || c == '[') depth++;
        if (c == '}' || c == ')' || c == ']') depth--;
    }
    return depth > 0;
}

static void repl() {
    // Legacy repl — kept for reference but unused
}

static bool ends_with(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static int run_ako_file(const std::string& path) {
    akar::ObjectFileReader reader;
    akar::ObjFunction* func = reader.read(path);
    if (!func) {
        std::cerr << "Error: Cannot read .ako file '" << path << "'" << std::endl;
        return 1;
    }
    akar::VM vm;
    auto result = vm.run_function(func);
    if (result == akar::InterpretResult::RuntimeError) {
        std::cerr << "Runtime error: " << vm.last_error() << std::endl;
        return 1;
    }
    return 0;
}

static int run_ak_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file '" << path << "'" << std::endl;
        return 1;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    std::string source = ss.str();
    file.close();

    // Tokenize
    akar::Lexer lexer(source);
    auto tokens = lexer.tokenize();

    // Parse
    akar::Parser parser(tokens);
    akar::ASTPtr ast;
    try {
        ast = parser.parse_program();
    } catch (const std::exception& e) {
        std::cerr << "Parse error: " << e.what() << std::endl;
        return 1;
    }

    // Compile with base path for includes
    akar::CodeGenerator codegen;
    size_t last_slash = path.rfind('/');
    if (last_slash != std::string::npos) {
        codegen.set_base_path(path.substr(0, last_slash));
    }
    akar::ObjFunction* func;
    try {
        func = codegen.compile(ast);
    } catch (const std::exception& e) {
        std::cerr << "Compile error: " << e.what() << std::endl;
        return 1;
    }

    // Run
    akar::VM vm;
    auto result = vm.run_function(func);
    if (result == akar::InterpretResult::RuntimeError) {
        std::cerr << "Runtime error: " << vm.last_error() << std::endl;
        return 1;
    }
    return 0;
}

int main(int argc, char* argv[]) {
    // Parse flags
    bool verbose = false;
    bool profile = false;
    bool trace = false;
    std::string file_path;
    std::string eval_code;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-p" || arg == "--profile") {
            profile = true;
        } else if (arg == "-t" || arg == "--trace") {
            trace = true;
        } else if (arg == "-e" || arg == "--eval") {
            if (i + 1 < argc) {
                eval_code = argv[++i];
            } else {
                std::cerr << "Error: -e requires an argument" << std::endl;
                return 1;
            }
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: akar [options] [file.ak | file.ako]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -v, --verbose   Enable verbose VM tracing" << std::endl;
            std::cout << "  -p, --profile   Enable profiling (report on exit)" << std::endl;
            std::cout << "  -t, --trace     Enable event tracing (dump on exit)" << std::endl;
            std::cout << "  -e, --eval CODE Evaluate code string" << std::endl;
            std::cout << "  -h, --help      Show this help" << std::endl;
            return 0;
        } else {
            file_path = arg;
        }
    }

    if (!eval_code.empty()) {
        akar::VM vm;
        vm.set_verbose(verbose);
        if (profile) vm.set_profiling(true);
        if (trace) vm.set_tracing(true);
        auto result = vm.interpret(eval_code);
        if (trace) vm.profiler_.print_trace_log();
        if (profile) vm.profiler_.print_profile_report();
        if (result == akar::InterpretResult::CompileError) {
            std::cerr << "Compile error: " << vm.last_error() << std::endl;
            return 1;
        } else if (result == akar::InterpretResult::RuntimeError) {
            std::cerr << "Runtime error: " << vm.last_error() << std::endl;
            return 1;
        }
        return 0;
    }

    if (!file_path.empty()) {
        if (ends_with(file_path, ".ako")) {
            akar::ObjectFileReader reader;
            akar::ObjFunction* func = reader.read(file_path);
            if (!func) {
                std::cerr << "Error: Cannot read .ako file '" << file_path << "'" << std::endl;
                return 1;
            }
            akar::VM vm;
            vm.set_verbose(verbose);
            if (profile) vm.set_profiling(true);
            if (trace) vm.set_tracing(true);
            auto result = vm.run_function(func);
            if (trace) vm.profiler_.print_trace_log();
            if (profile) vm.profiler_.print_profile_report();
            if (result == akar::InterpretResult::RuntimeError) {
                std::cerr << "Runtime error: " << vm.last_error() << std::endl;
                return 1;
            }
            return 0;
        }
        // .ak file
        std::ifstream file(file_path);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file '" << file_path << "'" << std::endl;
            return 1;
        }
        std::stringstream ss;
        ss << file.rdbuf();
        std::string source = ss.str();
        file.close();

        akar::Lexer lexer(source);
        auto tokens = lexer.tokenize();
        akar::Parser parser(tokens);
        akar::ASTPtr ast;
        try {
            ast = parser.parse_program();
        } catch (const std::exception& e) {
            std::cerr << "Parse error: " << e.what() << std::endl;
            return 1;
        }

        akar::CodeGenerator codegen;
        size_t last_slash = file_path.rfind('/');
        if (last_slash != std::string::npos) {
            codegen.set_base_path(file_path.substr(0, last_slash));
        }
        akar::ObjFunction* func;
        try {
            func = codegen.compile(ast);
        } catch (const std::exception& e) {
            std::cerr << "Compile error: " << e.what() << std::endl;
            return 1;
        }

        akar::VM vm;
        vm.set_verbose(verbose);
        if (profile) vm.set_profiling(true);
        if (trace) vm.set_tracing(true);
        auto result = vm.run_function(func);
        if (trace) vm.profiler_.print_trace_log();
        if (profile) vm.profiler_.print_profile_report();
        if (result == akar::InterpretResult::RuntimeError) {
            std::cerr << "Runtime error: " << vm.last_error() << std::endl;
            return 1;
        }
        return 0;
    }

    // REPL mode
    akar::VM vm;
    vm.set_verbose(verbose);
    if (profile) vm.set_profiling(true);
    if (trace) vm.set_tracing(true);

    akar::LineEditor editor;
    std::cout << "Akar Script v0.1.0" << std::endl;
    std::cout << "Type 'exit' to quit. Arrow keys, history, and tab completion supported." << std::endl;

    while (true) {
        std::string input;
        if (!editor.readline("> ", input)) break;  // EOF (Ctrl+D on empty line)
        if (input == "exit" || input == "quit") break;
        if (input.empty()) continue;

        // Multi-line: accumulate until expression is complete
        while (is_incomplete(input)) {
            std::string more;
            if (!editor.readline(".. ", more)) break;
            input += "\n" + more;
        }

        auto result = vm.interpret(input);
        if (result == akar::InterpretResult::CompileError) {
            std::cerr << "Compile error: " << vm.last_error() << std::endl;
        } else if (result == akar::InterpretResult::RuntimeError) {
            std::cerr << "Runtime error: " << vm.last_error() << std::endl;
        }
    }
    return 0;
}
