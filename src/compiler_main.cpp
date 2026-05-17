#include "akar/compiler/lexer.h"
#include "akar/compiler/parser.h"
#include "akar/compiler/codegen.h"
#include "akar/vm/object_file.h"
#include <iostream>
#include <fstream>
#include <sstream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: akarc <source.ak> [-o output.ako] [--release]" << std::endl;
        return 1;
    }

    std::string source_path;
    std::string output_path = "out.ako";
    bool strip_symbols = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--release") {
            strip_symbols = true;
        } else if (arg == "-o" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (source_path.empty()) {
            source_path = arg;
        } else if (output_path == "out.ako") {
            output_path = arg;
        }
    }

    if (source_path.empty()) {
        std::cerr << "Usage: akarc <source.ak> [-o output.ako] [--release]" << std::endl;
        return 1;
    }

    // Read source file
    std::ifstream file(source_path);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file '" << source_path << "'" << std::endl;
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

    // Compile
    akar::CodeGenerator codegen;
    // Set base path for include resolution (directory of source file)
    size_t last_slash = source_path.rfind('/');
    if (last_slash != std::string::npos) {
        codegen.set_base_path(source_path.substr(0, last_slash));
    }
    akar::ObjFunction* func;
    try {
        func = codegen.compile(ast);
    } catch (const std::exception& e) {
        std::cerr << "Compile error: " << e.what() << std::endl;
        return 1;
    }

    // Write .ako file
    akar::ObjectFileWriter writer;
    if (!writer.write(output_path, func, strip_symbols, codegen.identifier_values(), codegen.literal_values())) {
        std::cerr << "Error: Cannot write output file '" << output_path << "'" << std::endl;
        return 1;
    }

    std::cout << "Compiled " << source_path << " -> " << output_path;
    if (strip_symbols) std::cout << " (release, symbols stripped)";
    std::cout << std::endl;
    std::cout << "Bytecode size: " << func->bytecode.size() << " bytes" << std::endl;
    std::cout << "Constants: " << func->constants.size() << std::endl;
    return 0;
}
