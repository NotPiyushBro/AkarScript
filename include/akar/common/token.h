#pragma once
#include <string>

namespace akar {

enum class TokenType {
    // Literals
    Number, String, True, False, Nil,

    // Identifiers & keywords
    Identifier,
    Let, Fn, If, Else, While, For, In, Return, Break, Continue,
    Class, This, Super, New, And, Or, Not, Include, Await,
    Switch, Case, Default, Try, Catch, Throw,

    // Operators
    Plus, Minus, Star, Slash, Percent,
    Equal, EqualEqual, Bang, BangEqual,
    Less, LessEqual, Greater, GreaterEqual,
    Amp, AmpAmp, Pipe, PipePipe,
    Dot, DotDot, DotDotDot, // range, varargs

    // Delimiters
    LeftParen, RightParen,
    LeftBrace, RightBrace,
    LeftBracket, RightBracket,
    Comma, Colon, Semicolon,
    Arrow, // ->

    // Special
    Eof, Error
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
    double number_value;  // for Number tokens
    std::string string_value; // for String tokens

    Token() : type(TokenType::Eof), line(0), column(0), number_value(0) {}
    Token(TokenType t, std::string lex, int l, int c)
        : type(t), lexeme(std::move(lex)), line(l), column(c), number_value(0) {}
};

const char* token_type_name(TokenType type);

} // namespace akar
