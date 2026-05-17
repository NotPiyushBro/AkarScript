#pragma once
#include "akar/common/token.h"
#include <string>
#include <vector>

namespace akar {

class Lexer {
public:
    explicit Lexer(const std::string& source);
    std::vector<Token> tokenize();

private:
    Token next_token();
    Token make_token(TokenType type);
    Token make_token(TokenType type, const std::string& lexeme);
    Token error_token(const std::string& message);

    char advance();
    char peek() const;
    char peek_next() const;
    bool match(char expected);
    bool is_at_end() const;

    void skip_whitespace();
    Token read_string();
    Token read_number();
    Token read_identifier();

    TokenType check_keyword(int start, int length, const std::string& rest, TokenType type) const;
    TokenType identifier_type() const;

    std::string source_;
    int start_ = 0;
    int current_ = 0;
    int line_ = 1;
    int column_ = 1;
    int start_column_ = 1;
};

} // namespace akar
