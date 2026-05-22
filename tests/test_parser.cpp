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

// Test: Enum declaration
TEST(parser_enum_declaration) {
    akar::Lexer lexer("enum Color { Red, Green, Blue }");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    ASSERT_EQ(block->statements.size(), 1u);
    ASSERT_TRUE(block->statements[0]->type == akar::NodeType::EnumStmt);
}

// Test: Signal declaration
TEST(parser_signal_declaration) {
    akar::Lexer lexer("signal counter = 0");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    ASSERT_EQ(block->statements.size(), 1u);
    ASSERT_TRUE(block->statements[0]->type == akar::NodeType::SignalDeclStmt);
}

// Test: Effect statement
TEST(parser_effect_statement) {
    akar::Lexer lexer("effect { print(x) }");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    ASSERT_EQ(block->statements.size(), 1u);
    ASSERT_TRUE(block->statements[0]->type == akar::NodeType::EffectStmt);
}

// Test: Switch statement
TEST(parser_switch_statement) {
    akar::Lexer lexer("switch (x) { case 1: print(1) case 2: print(2) default: print(0) }");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    ASSERT_EQ(block->statements.size(), 1u);
    ASSERT_TRUE(block->statements[0]->type == akar::NodeType::SwitchStmt);
}

// Test: Try/catch statement
TEST(parser_try_catch) {
    akar::Lexer lexer("try { foo() } catch (e) { print(e) }");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    ASSERT_EQ(block->statements.size(), 1u);
    ASSERT_TRUE(block->statements[0]->type == akar::NodeType::TryCatchStmt);
}

// Test: Throw statement
TEST(parser_throw_statement) {
    akar::Lexer lexer("throw \"error\"");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    ASSERT_EQ(block->statements.size(), 1u);
    ASSERT_TRUE(block->statements[0]->type == akar::NodeType::ThrowStmt);
}

// Test: Bitwise AND expression
TEST(parser_bitwise_and) {
    akar::Lexer lexer("let x = a & b");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    ASSERT_EQ(block->statements.size(), 1u);
    ASSERT_TRUE(block->statements[0]->type == akar::NodeType::LetStmt);
    auto* let_stmt = static_cast<akar::LetStmt*>(block->statements[0].get());
    ASSERT_TRUE(let_stmt->initializer->type == akar::NodeType::Binary);
    auto* bin = static_cast<akar::BinaryExpr*>(let_stmt->initializer.get());
    ASSERT_EQ(bin->op, "&");
}

// Test: Bitwise OR expression
TEST(parser_bitwise_or) {
    akar::Lexer lexer("let x = a | b");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    auto* let_stmt = static_cast<akar::LetStmt*>(block->statements[0].get());
    auto* bin = static_cast<akar::BinaryExpr*>(let_stmt->initializer.get());
    ASSERT_EQ(bin->op, "|");
}

// Test: Shift expression
TEST(parser_shift_expression) {
    akar::Lexer lexer("let x = a << 2 >> 1");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    auto* let_stmt = static_cast<akar::LetStmt*>(block->statements[0].get());
    ASSERT_TRUE(let_stmt->initializer->type == akar::NodeType::Binary);
    auto* outer = static_cast<akar::BinaryExpr*>(let_stmt->initializer.get());
    ASSERT_EQ(outer->op, ">>");
    auto* inner = static_cast<akar::BinaryExpr*>(outer->left.get());
    ASSERT_EQ(inner->op, "<<");
}

// Test: Unary bitwise NOT
TEST(parser_bitwise_not) {
    akar::Lexer lexer("let x = ~a");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    auto* let_stmt = static_cast<akar::LetStmt*>(block->statements[0].get());
    ASSERT_TRUE(let_stmt->initializer->type == akar::NodeType::Unary);
    auto* unary = static_cast<akar::UnaryExpr*>(let_stmt->initializer.get());
    ASSERT_EQ(unary->op, "~");
}

// Test: Include declaration
TEST(parser_include_declaration) {
    akar::Lexer lexer("include \"math_utils.ak\"");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    ASSERT_EQ(block->statements.size(), 1u);
    ASSERT_TRUE(block->statements[0]->type == akar::NodeType::IncludeStmt);
}

// Test: Class with methods
TEST(parser_class_with_methods) {
    akar::Lexer lexer("class Vec { init(x, y) { this.x = x this.y = y } len() { return this.x + this.y } }");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    ASSERT_EQ(block->statements.size(), 1u);
    ASSERT_TRUE(block->statements[0]->type == akar::NodeType::ClassStmt);
}

// Test: Nested function
TEST(parser_nested_function) {
    akar::Lexer lexer("fn outer() { fn inner() { return 1 } return inner() }");
    auto tokens = lexer.tokenize();
    akar::Parser parser(tokens);
    auto ast = parser.parse_program();
    auto* block = static_cast<akar::BlockStmt*>(ast.get());
    ASSERT_EQ(block->statements.size(), 1u);
    ASSERT_TRUE(block->statements[0]->type == akar::NodeType::FnStmt);
}
