#include "akar/vm/vm.h"
#include "akar/vm/object_file.h"
#include "akar/compiler/lexer.h"
#include "akar/compiler/parser.h"
#include "akar/compiler/codegen.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

static void repl() {
    akar::VM vm;
    std::cout << "Akar Script v0.1.0" << std::endl;
    std::cout << "Type 'exit' to quit." << std::endl;

    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "exit" || line == "quit") break;
        if (line.empty()) continue;

        auto result = vm.interpret(line);
        if (result == akar::InterpretResult::CompileError) {
            std::cerr << "Compile error: " << vm.last_error() << std::endl;
        } else if (result == akar::InterpretResult::RuntimeError) {
            std::cerr << "Runtime error: " << vm.last_error() << std::endl;
        }
    }
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
    std::string file_path;
    std::string eval_code;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose") {
            verbose = true;
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
        auto result = vm.interpret(eval_code);
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
            auto result = vm.run_function(func);
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
        auto result = vm.run_function(func);
        if (result == akar::InterpretResult::RuntimeError) {
            std::cerr << "Runtime error: " << vm.last_error() << std::endl;
            return 1;
        }
        return 0;
    }

    // REPL mode
    akar::VM vm;
    vm.set_verbose(verbose);
    std::cout << "Akar Script v0.1.0" << std::endl;
    std::cout << "Type 'exit' to quit." << std::endl;

    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        if (line == "exit" || line == "quit") break;
        if (line.empty()) continue;

        auto result = vm.interpret(line);
        if (result == akar::InterpretResult::CompileError) {
            std::cerr << "Compile error: " << vm.last_error() << std::endl;
        } else if (result == akar::InterpretResult::RuntimeError) {
            std::cerr << "Runtime error: " << vm.last_error() << std::endl;
        }
    }
    return 0;
}
