#include "akar/compiler/lexer.h"

TEST(lexer_basic_tokens) {
    akar::Lexer lexer("let x = 10 + 20");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens[0].type, akar::TokenType::Let);
    ASSERT_EQ(tokens[1].type, akar::TokenType::Identifier);
    ASSERT_EQ(tokens[1].lexeme, "x");
    ASSERT_EQ(tokens[2].type, akar::TokenType::Equal);
    ASSERT_EQ(tokens[3].type, akar::TokenType::Number);
    ASSERT_EQ(tokens[3].number_value, 10.0);
    ASSERT_EQ(tokens[4].type, akar::TokenType::Plus);
    ASSERT_EQ(tokens[5].type, akar::TokenType::Number);
    ASSERT_EQ(tokens[5].number_value, 20.0);
}

TEST(lexer_strings) {
    akar::Lexer lexer("\"hello world\"");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens[0].type, akar::TokenType::String);
    ASSERT_EQ(tokens[0].string_value, "hello world");
}

TEST(lexer_keywords) {
    akar::Lexer lexer("if else while for in fn class return break continue");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens[0].type, akar::TokenType::If);
    ASSERT_EQ(tokens[1].type, akar::TokenType::Else);
    ASSERT_EQ(tokens[2].type, akar::TokenType::While);
    ASSERT_EQ(tokens[3].type, akar::TokenType::For);
    ASSERT_EQ(tokens[4].type, akar::TokenType::In);
    ASSERT_EQ(tokens[5].type, akar::TokenType::Fn);
    ASSERT_EQ(tokens[6].type, akar::TokenType::Class);
    ASSERT_EQ(tokens[7].type, akar::TokenType::Return);
    ASSERT_EQ(tokens[8].type, akar::TokenType::Break);
    ASSERT_EQ(tokens[9].type, akar::TokenType::Continue);
}

TEST(lexer_operators) {
    akar::Lexer lexer("== != <= >= && || .. ->");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens[0].type, akar::TokenType::EqualEqual);
    ASSERT_EQ(tokens[1].type, akar::TokenType::BangEqual);
    ASSERT_EQ(tokens[2].type, akar::TokenType::LessEqual);
    ASSERT_EQ(tokens[3].type, akar::TokenType::GreaterEqual);
    ASSERT_EQ(tokens[4].type, akar::TokenType::AmpAmp);
    ASSERT_EQ(tokens[5].type, akar::TokenType::PipePipe);
    ASSERT_EQ(tokens[6].type, akar::TokenType::DotDot);
    ASSERT_EQ(tokens[7].type, akar::TokenType::Arrow);
}

TEST(lexer_comments) {
    akar::Lexer lexer("let x = 10 // this is a comment\nlet y = 20");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens[0].type, akar::TokenType::Let);
    ASSERT_EQ(tokens[3].type, akar::TokenType::Number);
    ASSERT_EQ(tokens[4].type, akar::TokenType::Let);
}

TEST(lexer_booleans) {
    akar::Lexer lexer("true false nil");
    auto tokens = lexer.tokenize();
    ASSERT_EQ(tokens[0].type, akar::TokenType::True);
    ASSERT_EQ(tokens[1].type, akar::TokenType::False);
    ASSERT_EQ(tokens[2].type, akar::TokenType::Nil);
}
