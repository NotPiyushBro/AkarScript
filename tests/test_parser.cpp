#include "akar/compiler/lexer.h"
#include "akar/compiler/parser.h"
#include "akar/compiler/ast.h"

TEST(parser_let_declaration) {
    akar::Lexer lexer("let x = 10");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    ASSERT_TRUE(ast->type == akar::NodeType::Block);
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    ASSERT_EQ(block->statements.size(), 1u);
    ASSERT_TRUE(block->statements[0]->type == akar::NodeType::LetStmt);
}

TEST(parser_function_declaration) {
    akar::Lexer lexer("fn add(a, b) { return a + b }");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    ASSERT_EQ(block->statements.size(), 1u);
    ASSERT_TRUE(block->statements[0]->type == akar::NodeType::FnStmt);
}

TEST(parser_if_statement) {
    akar::Lexer lexer("if (x > 0) { print(x) }");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    ASSERT_EQ(block->statements.size(), 1u);
    ASSERT_TRUE(block->statements[0]->type == akar::NodeType::IfStmt);
}

TEST(parser_while_loop) {
    akar::Lexer lexer("while (x < 10) { x = x + 1 }");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    ASSERT_EQ(block->statements.size(), 1u);
    ASSERT_TRUE(block->statements[0]->type == akar::NodeType::WhileStmt);
}

TEST(parser_for_in_loop) {
    akar::Lexer lexer("for i in 0..10 { print(i) }");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    ASSERT_EQ(block->statements.size(), 1u);
    ASSERT_TRUE(block->statements[0]->type == akar::NodeType::ForInStmt);
}

TEST(parser_array_literal) {
    akar::Lexer lexer("let arr = [1, 2, 3]");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    ASSERT_EQ(block->statements.size(), 1u);
    ASSERT_TRUE(block->statements[0]->type == akar::NodeType::LetStmt);
}

TEST(parser_class_declaration) {
    akar::Lexer lexer("class Foo { bar() { } }");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    ASSERT_EQ(block->statements.size(), 1u);
    ASSERT_TRUE(block->statements[0]->type == akar::NodeType::ClassStmt);
}

TEST(parser_lambda) {
    akar::Lexer lexer("let f = fn(x) { x * 2 }");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    ASSERT_EQ(block->statements.size(), 1u);
    ASSERT_TRUE(block->statements[0]->type == akar::NodeType::LetStmt);
}
