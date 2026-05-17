#pragma once
#include "lexer.h"
#include "ast.h"
#include <vector>
#include <string>

namespace akar {

class Parser {
public:
    Parser(const std::vector<Token>& tokens);
    ASTPtr parse_program();

private:
    // Statements
    ASTPtr declaration();
    ASTPtr statement();
    ASTPtr let_declaration();
    ASTPtr fn_declaration();
    ASTPtr class_declaration();
    ASTPtr include_declaration();
    ASTPtr await_statement();
    ASTPtr switch_statement();
    ASTPtr try_statement();
    ASTPtr throw_statement();
    ASTPtr if_statement();
    ASTPtr while_statement();
    ASTPtr for_statement();
    ASTPtr return_statement();
    ASTPtr break_statement();
    ASTPtr continue_statement();
    ASTPtr expression_statement();
    ASTPtr block();

    // Expressions
    ASTPtr expression();
    ASTPtr assignment();
    ASTPtr or_expr();
    ASTPtr and_expr();
    ASTPtr equality();
    ASTPtr comparison();
    ASTPtr range();
    ASTPtr addition();
    ASTPtr multiplication();
    ASTPtr unary();
    ASTPtr call();
    ASTPtr primary();

    // Helpers
    ASTPtr finish_call(ASTPtr callee);
    ASTPtr finish_index(ASTPtr object);

    const Token& peek() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool match_any(std::initializer_list<TokenType> types);
    const Token& expect(TokenType type, const std::string& message);
    bool is_at_end() const;
    Token previous() const;

    void error(const std::string& message);
    void synchronize();

    std::vector<Token> tokens_;
    int current_ = 0;
    std::vector<std::string> errors_;
};

} // namespace akar
