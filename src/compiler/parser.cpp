#include "akar/compiler/parser.h"
#include <stdexcept>
#include <iostream>

namespace akar {

Parser::Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}

ASTPtr Parser::parse_program() {
    auto block = std::make_shared<BlockStmt>(0);
    while (!is_at_end()) {
        auto decl = declaration();
        if (decl) block->statements.push_back(decl);
    }
    return block;
}

ASTPtr Parser::declaration() {
    if (match(TokenType::Let)) return let_declaration();
    if (match(TokenType::Fn)) return fn_declaration();
    if (match(TokenType::Class)) return class_declaration();
    if (match(TokenType::Include)) return include_declaration();
    if (match(TokenType::Signal)) return signal_declaration();
    if (match(TokenType::Effect)) return effect_statement();
    if (match(TokenType::Enum)) return enum_declaration();
    return statement();
}

ASTPtr Parser::statement() {
    if (match(TokenType::If)) return if_statement();
    if (match(TokenType::While)) return while_statement();
    if (match(TokenType::For)) return for_statement();
    if (match(TokenType::Return)) return return_statement();
    if (match(TokenType::Break)) return break_statement();
    if (match(TokenType::Continue)) return continue_statement();
    if (match(TokenType::Await)) return await_statement();
    if (match(TokenType::Switch)) return switch_statement();
    if (match(TokenType::Try)) return try_statement();
    if (match(TokenType::Throw)) return throw_statement();
    if (match(TokenType::LeftBrace)) return block();
    return expression_statement();
}

ASTPtr Parser::expression_statement() {
    auto expr = expression();
    // Semicolons are optional (newline-terminated)
    match(TokenType::Semicolon);
    return std::make_shared<ExprStmt>(expr, expr->line);
}

ASTPtr Parser::let_declaration() {
    // Destructuring: let [a, b, c] = expr
    if (match(TokenType::LeftBracket)) {
        std::vector<std::string> names;
        if (!check(TokenType::RightBracket)) {
            do {
                auto name_tok = expect(TokenType::Identifier, "Expected variable name in destructuring");
                names.push_back(name_tok.lexeme);
            } while (match(TokenType::Comma));
        }
        expect(TokenType::RightBracket, "Expected ']' after destructuring names");
        expect(TokenType::Equal, "Expected '=' after destructuring");
        auto init = expression();
        match(TokenType::Semicolon);
        return std::make_shared<DestructuringStmt>(std::move(names), init, init->line);
    }

    auto name_tok = expect(TokenType::Identifier, "Expected variable name");
    ASTPtr init = nullptr;
    if (match(TokenType::Equal)) {
        init = expression();
    }
    match(TokenType::Semicolon);
    return std::make_shared<LetStmt>(name_tok.lexeme, init, name_tok.line);
}

ASTPtr Parser::fn_declaration() {
    auto name_tok = expect(TokenType::Identifier, "Expected function name");
    expect(TokenType::LeftParen, "Expected '(' after function name");
    std::vector<Param> params;
    if (!check(TokenType::RightParen)) {
        do {
            if (match(TokenType::DotDotDot)) {
                auto param = expect(TokenType::Identifier, "Expected variadic parameter name");
                params.push_back({param.lexeme, true});
                break; // variadic must be last
            }
            auto param = expect(TokenType::Identifier, "Expected parameter name");
            params.push_back({param.lexeme, false});
        } while (match(TokenType::Comma));
    }
    expect(TokenType::RightParen, "Expected ')' after parameters");
    expect(TokenType::LeftBrace, "Expected '{' before function body");
    auto body = block();
    return std::make_shared<FnStmt>(name_tok.lexeme, std::move(params), body, name_tok.line);
}

ASTPtr Parser::class_declaration() {
    auto name_tok = expect(TokenType::Identifier, "Expected class name");
    expect(TokenType::LeftBrace, "Expected '{' before class body");
    auto cls = std::make_shared<ClassStmt>(name_tok.lexeme, name_tok.line);
    while (!check(TokenType::RightBrace) && !is_at_end()) {
        auto method_name = expect(TokenType::Identifier, "Expected method name");
        expect(TokenType::LeftParen, "Expected '(' after method name");
        std::vector<Param> params;
        if (!check(TokenType::RightParen)) {
            do {
                auto param = expect(TokenType::Identifier, "Expected parameter name");
                params.push_back({param.lexeme});
            } while (match(TokenType::Comma));
        }
        expect(TokenType::RightParen, "Expected ')' after parameters");
        expect(TokenType::LeftBrace, "Expected '{' before method body");
        auto body = block();
        cls->methods.push_back(std::make_shared<FnStmt>(method_name.lexeme, std::move(params), body, method_name.line));
    }
    expect(TokenType::RightBrace, "Expected '}' after class body");
    return cls;
}

ASTPtr Parser::include_declaration() {
    auto path_tok = expect(TokenType::String, "Expected file path string after 'include'");
    match(TokenType::Semicolon);
    return std::make_shared<IncludeStmt>(path_tok.string_value, path_tok.line);
}

ASTPtr Parser::signal_declaration() {
    int line = previous().line;
    auto name_tok = expect(TokenType::Identifier, "Expected signal name");
    expect(TokenType::Equal, "Expected '=' after signal name");
    auto init = expression();
    match(TokenType::Semicolon);
    return std::make_shared<SignalDeclStmt>(name_tok.lexeme, init, line);
}

ASTPtr Parser::effect_statement() {
    int line = previous().line;
    expect(TokenType::LeftBrace, "Expected '{' after 'effect'");
    auto body = block();
    return std::make_shared<EffectStmt>(body, line);
}

ASTPtr Parser::enum_declaration() {
    int line = previous().line;
    auto name_tok = expect(TokenType::Identifier, "Expected enum name");
    expect(TokenType::LeftBrace, "Expected '{' before enum body");
    auto node = std::make_shared<EnumStmt>(name_tok.lexeme, line);

    if (!check(TokenType::RightBrace)) {
        do {
            auto variant_name = expect(TokenType::Identifier, "Expected variant name");
            EnumVariant variant;
            variant.name = variant_name.lexeme;
            // Optional associated value: Variant(expr) or Variant = expr
            if (match(TokenType::LeftParen)) {
                variant.value = expression();
                expect(TokenType::RightParen, "Expected ')' after variant value");
            } else if (match(TokenType::Equal)) {
                variant.value = expression();
            }
            node->variants.push_back(std::move(variant));
        } while (match(TokenType::Comma));
    }

    expect(TokenType::RightBrace, "Expected '}' after enum body");
    match(TokenType::Semicolon);
    return node;
}

ASTPtr Parser::await_statement() {
    int line = previous().line;
    auto expr = expression();
    match(TokenType::Semicolon);
    return std::make_shared<AwaitStmt>(expr, line);
}

ASTPtr Parser::switch_statement() {
    int line = previous().line;
    expect(TokenType::LeftParen, "Expected '(' after 'switch'");
    auto expr = expression();
    expect(TokenType::RightParen, "Expected ')' after switch expression");
    expect(TokenType::LeftBrace, "Expected '{' before switch body");

    auto node = std::make_shared<SwitchStmt>(expr, line);

    while (!check(TokenType::RightBrace) && !is_at_end()) {
        if (match(TokenType::Case)) {
            CaseClause clause;
            // case value1, value2:
            do {
                clause.values.push_back(expression());
            } while (match(TokenType::Comma));
            expect(TokenType::Colon, "Expected ':' after case values");
            // Parse body statements until next case/default/}
            auto body = std::make_shared<BlockStmt>(previous().line);
            while (!check(TokenType::Case) && !check(TokenType::Default) && !check(TokenType::RightBrace) && !is_at_end()) {
                body->statements.push_back(declaration());
            }
            clause.body = body;
            node->cases.push_back(std::move(clause));
        } else if (match(TokenType::Default)) {
            expect(TokenType::Colon, "Expected ':' after 'default'");
            auto body = std::make_shared<BlockStmt>(previous().line);
            while (!check(TokenType::Case) && !check(TokenType::Default) && !check(TokenType::RightBrace) && !is_at_end()) {
                body->statements.push_back(declaration());
            }
            node->default_body = body;
        } else {
            error("Expected 'case' or 'default' in switch body");
            break;
        }
    }

    expect(TokenType::RightBrace, "Expected '}' after switch body");
    return node;
}

ASTPtr Parser::try_statement() {
    int line = previous().line;
    expect(TokenType::LeftBrace, "Expected '{' after 'try'");
    auto try_body = block();

    expect(TokenType::Catch, "Expected 'catch' after try block");
    expect(TokenType::LeftParen, "Expected '(' after 'catch'");
    auto err_tok = expect(TokenType::Identifier, "Expected variable name in catch");
    expect(TokenType::RightParen, "Expected ')' after catch variable");
    expect(TokenType::LeftBrace, "Expected '{' before catch body");
    auto catch_body = block();

    return std::make_shared<TryCatchStmt>(try_body, err_tok.lexeme, catch_body, line);
}

ASTPtr Parser::throw_statement() {
    int line = previous().line;
    auto value = expression();
    match(TokenType::Semicolon);
    return std::make_shared<ThrowStmt>(value, line);
}

ASTPtr Parser::if_statement() {
    int line = previous().line;
    expect(TokenType::LeftParen, "Expected '(' after 'if'");
    auto condition = expression();
    expect(TokenType::RightParen, "Expected ')' after condition");
    auto then_branch = statement();
    ASTPtr else_branch = nullptr;
    if (match(TokenType::Else)) {
        else_branch = statement();
    }
    return std::make_shared<IfStmt>(condition, then_branch, else_branch, line);
}

ASTPtr Parser::while_statement() {
    int line = previous().line;
    expect(TokenType::LeftParen, "Expected '(' after 'while'");
    auto condition = expression();
    expect(TokenType::RightParen, "Expected ')' after condition");
    auto body = statement();
    return std::make_shared<WhileStmt>(condition, body, line);
}

ASTPtr Parser::for_statement() {
    int line = previous().line;

    // Check for "for x in collection" syntax (no parens)
    if (check(TokenType::Identifier) && current_ + 1 < (int)tokens_.size() &&
        tokens_[current_ + 1].type == TokenType::In) {
        auto var = advance().lexeme;
        advance(); // consume 'in'
        auto iterable = expression();
        auto body = statement();
        return std::make_shared<ForInStmt>(var, iterable, body, line);
    }

    // Regular for loop: for (init; cond; update)
    expect(TokenType::LeftParen, "Expected '(' after 'for'");
    ASTPtr init = nullptr;
    if (match(TokenType::Let)) {
        init = let_declaration();
    } else if (!check(TokenType::Semicolon)) {
        init = expression();
        match(TokenType::Semicolon);
    } else {
        match(TokenType::Semicolon);
    }

    ASTPtr cond = nullptr;
    if (!check(TokenType::Semicolon)) {
        cond = expression();
    }
    match(TokenType::Semicolon);

    ASTPtr update = nullptr;
    if (!check(TokenType::RightParen)) {
        update = expression();
    }
    expect(TokenType::RightParen, "Expected ')' after for clauses");
    auto body = statement();
    return std::make_shared<ForStmt>(init, cond, update, body, line);
}

ASTPtr Parser::return_statement() {
    int line = previous().line;
    std::vector<ASTPtr> values;
    if (!check(TokenType::Semicolon) && !check(TokenType::RightBrace) && !is_at_end()) {
        values.push_back(expression());
        while (match(TokenType::Comma)) {
            values.push_back(expression());
        }
    }
    match(TokenType::Semicolon);
    return std::make_shared<ReturnStmt>(std::move(values), line);
}

ASTPtr Parser::break_statement() {
    int line = previous().line;
    match(TokenType::Semicolon);
    return std::make_shared<BreakStmt>(line);
}

ASTPtr Parser::continue_statement() {
    int line = previous().line;
    match(TokenType::Semicolon);
    return std::make_shared<ContinueStmt>(line);
}

ASTPtr Parser::block() {
    auto block_node = std::make_shared<BlockStmt>(peek().line);
    while (!check(TokenType::RightBrace) && !is_at_end()) {
        auto decl = declaration();
        if (decl) block_node->statements.push_back(decl);
    }
    expect(TokenType::RightBrace, "Expected '}' after block");
    return block_node;
}

// Expression parsing (precedence climbing)
ASTPtr Parser::expression() {
    return assignment();
}

ASTPtr Parser::assignment() {
    auto expr = or_expr();
    if (match(TokenType::Equal)) {
        int line = previous().line;
        auto value = assignment();
        if (expr->type == NodeType::Identifier) {
            auto name = static_cast<Identifier*>(expr.get())->name;
            return std::make_shared<AssignmentExpr>(name, value, line);
        } else if (expr->type == NodeType::Index) {
            auto index = static_cast<IndexExpr*>(expr.get());
            return std::make_shared<ArrayIndexSetExpr>(index->object, index->index, value, line);
        } else if (expr->type == NodeType::FieldAccess) {
            auto field = static_cast<FieldAccessExpr*>(expr.get());
            return std::make_shared<FieldSetExpr>(field->object, field->field, value, line);
        }
        error("Invalid assignment target");
    }
    return expr;
}

ASTPtr Parser::or_expr() {
    auto expr = and_expr();
    while (match(TokenType::Or)) {
        int line = previous().line;
        auto right = and_expr();
        expr = std::make_shared<LogicalExpr>(expr, "or", right, line);
    }
    return expr;
}

ASTPtr Parser::and_expr() {
    auto expr = bitwise_or();
    while (match(TokenType::And)) {
        int line = previous().line;
        auto right = bitwise_or();
        expr = std::make_shared<LogicalExpr>(expr, "and", right, line);
    }
    return expr;
}

ASTPtr Parser::bitwise_or() {
    auto expr = bitwise_xor();
    while (match(TokenType::Pipe)) {
        auto op = previous().lexeme;
        int line = previous().line;
        auto right = bitwise_xor();
        expr = std::make_shared<BinaryExpr>(expr, op, right, line);
    }
    return expr;
}

ASTPtr Parser::bitwise_xor() {
    auto expr = bitwise_and();
    while (match(TokenType::Caret)) {
        auto op = previous().lexeme;
        int line = previous().line;
        auto right = bitwise_and();
        expr = std::make_shared<BinaryExpr>(expr, op, right, line);
    }
    return expr;
}

ASTPtr Parser::bitwise_and() {
    auto expr = equality();
    while (match(TokenType::Amp)) {
        auto op = previous().lexeme;
        int line = previous().line;
        auto right = equality();
        expr = std::make_shared<BinaryExpr>(expr, op, right, line);
    }
    return expr;
}

ASTPtr Parser::equality() {
    auto expr = comparison();
    while (match_any({TokenType::EqualEqual, TokenType::BangEqual})) {
        auto op = previous().lexeme;
        int line = previous().line;
        auto right = comparison();
        expr = std::make_shared<BinaryExpr>(expr, op, right, line);
    }
    return expr;
}

ASTPtr Parser::comparison() {
    auto expr = shift();
    while (match_any({TokenType::Less, TokenType::LessEqual, TokenType::Greater, TokenType::GreaterEqual})) {
        auto op = previous().lexeme;
        int line = previous().line;
        auto right = shift();
        expr = std::make_shared<BinaryExpr>(expr, op, right, line);
    }
    return expr;
}

ASTPtr Parser::shift() {
    auto expr = range();
    while (match_any({TokenType::LessLess, TokenType::GreaterGreater})) {
        auto op = previous().lexeme;
        int line = previous().line;
        auto right = range();
        expr = std::make_shared<BinaryExpr>(expr, op, right, line);
    }
    return expr;
}

ASTPtr Parser::range() {
    auto expr = addition();
    if (match(TokenType::DotDot)) {
        int line = previous().line;
        bool inclusive = match(TokenType::Equal);
        auto end = addition();
        expr = std::make_shared<RangeExpr>(expr, end, inclusive, line);
    }
    return expr;
}

ASTPtr Parser::addition() {
    auto expr = multiplication();
    while (match_any({TokenType::Plus, TokenType::Minus})) {
        auto op = previous().lexeme;
        int line = previous().line;
        auto right = multiplication();
        expr = std::make_shared<BinaryExpr>(expr, op, right, line);
    }
    return expr;
}

ASTPtr Parser::multiplication() {
    auto expr = unary();
    while (match_any({TokenType::Star, TokenType::Slash, TokenType::Percent})) {
        auto op = previous().lexeme;
        int line = previous().line;
        auto right = unary();
        expr = std::make_shared<BinaryExpr>(expr, op, right, line);
    }
    return expr;
}

ASTPtr Parser::unary() {
    if (match_any({TokenType::Minus, TokenType::Bang, TokenType::Not, TokenType::Tilde})) {
        auto op = previous().lexeme;
        int line = previous().line;
        auto operand = unary();
        return std::make_shared<UnaryExpr>(op, operand, line);
    }
    return call();
}

ASTPtr Parser::call() {
    auto expr = primary();
    while (true) {
        if (match(TokenType::LeftParen)) {
            expr = finish_call(expr);
        } else if (match(TokenType::LeftBracket)) {
            expr = finish_index(expr);
        } else if (match(TokenType::Dot)) {
            auto name = expect(TokenType::Identifier, "Expected property name after '.'");
            expr = std::make_shared<FieldAccessExpr>(expr, name.lexeme, name.line);
        } else {
            break;
        }
    }
    return expr;
}

ASTPtr Parser::finish_call(ASTPtr callee) {
    auto call_expr = std::make_shared<CallExpr>(callee, callee->line);
    if (!check(TokenType::RightParen)) {
        do {
            call_expr->arguments.push_back(expression());
        } while (match(TokenType::Comma));
    }
    expect(TokenType::RightParen, "Expected ')' after arguments");
    return call_expr;
}

ASTPtr Parser::finish_index(ASTPtr object) {
    auto index = expression();
    expect(TokenType::RightBracket, "Expected ']'");
    return std::make_shared<IndexExpr>(object, index, previous().line);
}

ASTPtr Parser::primary() {
    if (match(TokenType::Number)) {
        return std::make_shared<NumberLiteral>(previous().number_value, previous().line);
    }
    if (match(TokenType::String)) {
        return std::make_shared<StringLiteral>(previous().string_value, previous().line);
    }
    if (match(TokenType::True)) {
        return std::make_shared<BoolLiteral>(true, previous().line);
    }
    if (match(TokenType::False)) {
        return std::make_shared<BoolLiteral>(false, previous().line);
    }
    if (match(TokenType::Nil)) {
        return std::make_shared<NilLiteral>(previous().line);
    }
    if (match(TokenType::Identifier)) {
        return std::make_shared<Identifier>(previous().lexeme, previous().line);
    }
    if (match(TokenType::This)) {
        return std::make_shared<ThisExpr>(previous().line);
    }
    if (match(TokenType::Super)) {
        expect(TokenType::Dot, "Expected '.' after 'super'");
        auto method = expect(TokenType::Identifier, "Expected method name after 'super.'");
        return std::make_shared<SuperAccessExpr>(method.lexeme, method.line);
    }
    if (match(TokenType::LeftParen)) {
        auto expr = expression();
        expect(TokenType::RightParen, "Expected ')'");
        return expr;
    }
    if (match(TokenType::LeftBracket)) {
        auto arr = std::make_shared<ArrayLiteral>(previous().line);
        if (!check(TokenType::RightBracket)) {
            do {
                arr->elements.push_back(expression());
            } while (match(TokenType::Comma));
        }
        expect(TokenType::RightBracket, "Expected ']'");
        return arr;
    }
    if (match(TokenType::New)) {
        auto name = expect(TokenType::Identifier, "Expected class name after 'new'");
        expect(TokenType::LeftParen, "Expected '(' after class name");
        auto call_expr = std::make_shared<CallExpr>(
            std::make_shared<Identifier>(name.lexeme, name.line), name.line);
        if (!check(TokenType::RightParen)) {
            do {
                call_expr->arguments.push_back(expression());
            } while (match(TokenType::Comma));
        }
        expect(TokenType::RightParen, "Expected ')' after arguments");
        return call_expr;
    }
    if (match(TokenType::Fn)) {
        // Lambda: fn(params) { body }
        expect(TokenType::LeftParen, "Expected '(' after 'fn'");
        std::vector<Param> params;
        if (!check(TokenType::RightParen)) {
            do {
                if (match(TokenType::DotDotDot)) {
                    auto param = expect(TokenType::Identifier, "Expected variadic parameter name");
                    params.push_back({param.lexeme, true});
                    break;
                }
                auto param = expect(TokenType::Identifier, "Expected parameter name");
                params.push_back({param.lexeme, false});
            } while (match(TokenType::Comma));
        }
        expect(TokenType::RightParen, "Expected ')' after parameters");
        expect(TokenType::LeftBrace, "Expected '{' before lambda body");
        auto body = block();
        return std::make_shared<FnStmt>("", std::move(params), body, previous().line);
    }
    if (match(TokenType::LeftBrace)) {
        auto map = std::make_shared<MapLiteral>(previous().line);
        if (!check(TokenType::RightBrace)) {
            do {
                auto key = expression();
                expect(TokenType::Colon, "Expected ':' after map key");
                auto value = expression();
                map->entries.push_back({std::move(key), std::move(value)});
            } while (match(TokenType::Comma));
        }
        expect(TokenType::RightBrace, "Expected '}' after map entries");
        return map;
    }

    error("Expected expression");
    return std::make_shared<NilLiteral>(0);
}

// Helpers
const Token& Parser::peek() const { return tokens_[current_]; }
const Token& Parser::advance() { if (!is_at_end()) current_++; return tokens_[current_ - 1]; }
bool Parser::check(TokenType type) const { return !is_at_end() && peek().type == type; }

bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

bool Parser::match_any(std::initializer_list<TokenType> types) {
    for (auto type : types) {
        if (check(type)) { advance(); return true; }
    }
    return false;
}

const Token& Parser::expect(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    error(message);
    return tokens_[current_]; // unreachable but satisfies compiler
}

bool Parser::is_at_end() const { return peek().type == TokenType::Eof; }
Token Parser::previous() const { return tokens_[current_ - 1]; }

void Parser::error(const std::string& message) {
    auto& tok = peek();
    std::string err = "[Line " + std::to_string(tok.line) + ":" + std::to_string(tok.column) + "] " + message;
    errors_.push_back(err);
    throw std::runtime_error(err);
}

void Parser::synchronize() {
    while (!is_at_end()) {
        if (previous().type == TokenType::Semicolon) return;
        switch (peek().type) {
            case TokenType::Class: case TokenType::Fn: case TokenType::Let:
            case TokenType::For: case TokenType::If: case TokenType::While:
            case TokenType::Return: case TokenType::Break: case TokenType::Continue:
            case TokenType::Include: case TokenType::Await:
            case TokenType::Switch: case TokenType::Try: case TokenType::Throw:
            case TokenType::Signal: case TokenType::Effect: case TokenType::Enum:
                return;
            default: advance(); break;
        }
    }
}

} // namespace akar
