#include "akar/compiler/lexer.h"
#include <cctype>
#include <cstdlib>

namespace akar {

const char* token_type_name(TokenType type) {
    switch (type) {
        case TokenType::Number: return "Number";
        case TokenType::String: return "String";
        case TokenType::True: return "True";
        case TokenType::False: return "False";
        case TokenType::Nil: return "Nil";
        case TokenType::Identifier: return "Identifier";
        case TokenType::Let: return "Let";
        case TokenType::Fn: return "Fn";
        case TokenType::If: return "If";
        case TokenType::Else: return "Else";
        case TokenType::While: return "While";
        case TokenType::For: return "For";
        case TokenType::In: return "In";
        case TokenType::Return: return "Return";
        case TokenType::Break: return "Break";
        case TokenType::Continue: return "Continue";
        case TokenType::Class: return "Class";
        case TokenType::This: return "This";
        case TokenType::Super: return "Super";
        case TokenType::New: return "New";
        case TokenType::And: return "And";
        case TokenType::Or: return "Or";
        case TokenType::Not: return "Not";
        case TokenType::Include: return "Include";
        case TokenType::Await: return "Await";
        case TokenType::Switch: return "Switch";
        case TokenType::Case: return "Case";
        case TokenType::Default: return "Default";
        case TokenType::Try: return "Try";
        case TokenType::Catch: return "Catch";
        case TokenType::Throw: return "Throw";
        case TokenType::Signal: return "Signal";
        case TokenType::Effect: return "Effect";
        case TokenType::Enum: return "Enum";
        case TokenType::Plus: return "+";
        case TokenType::Minus: return "-";
        case TokenType::Star: return "*";
        case TokenType::Slash: return "/";
        case TokenType::Percent: return "%";
        case TokenType::Equal: return "=";
        case TokenType::EqualEqual: return "==";
        case TokenType::Bang: return "!";
        case TokenType::BangEqual: return "!=";
        case TokenType::Less: return "<";
        case TokenType::LessEqual: return "<=";
        case TokenType::Greater: return ">";
        case TokenType::GreaterEqual: return ">=";
        case TokenType::Amp: return "&";
        case TokenType::AmpAmp: return "&&";
        case TokenType::Pipe: return "|";
        case TokenType::PipePipe: return "||";
        case TokenType::Caret: return "^";
        case TokenType::Tilde: return "~";
        case TokenType::LessLess: return "<<";
        case TokenType::GreaterGreater: return ">>";
        case TokenType::Dot: return ".";
        case TokenType::DotDot: return "..";
        case TokenType::DotDotDot: return "...";
        case TokenType::LeftParen: return "(";
        case TokenType::RightParen: return ")";
        case TokenType::LeftBrace: return "{";
        case TokenType::RightBrace: return "}";
        case TokenType::LeftBracket: return "[";
        case TokenType::RightBracket: return "]";
        case TokenType::Comma: return ",";
        case TokenType::Colon: return ":";
        case TokenType::Semicolon: return ";";
        case TokenType::Arrow: return "->";
        case TokenType::At: return "@";
        case TokenType::Eof: return "EOF";
        case TokenType::Error: return "Error";
    }
    return "Unknown";
}

Lexer::Lexer(const std::string& source) : source_(source) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (!is_at_end()) {
        start_ = current_;
        Token tok = next_token();
        tokens.push_back(tok);
        if (tok.type == TokenType::Error) break;
    }
    tokens.emplace_back(TokenType::Eof, "", line_, column_);
    return tokens;
}

Token Lexer::next_token() {
    skip_whitespace();
    if (is_at_end()) return make_token(TokenType::Eof);

    start_ = current_;
    start_column_ = column_;
    char c = advance();

    if (std::isalpha(c) || c == '_') return read_identifier();
    if (std::isdigit(c)) return read_number();
    if (c == '"' || c == '\'') return read_string();

    switch (c) {
        case '(': return make_token(TokenType::LeftParen);
        case ')': return make_token(TokenType::RightParen);
        case '{': return make_token(TokenType::LeftBrace);
        case '}': return make_token(TokenType::RightBrace);
        case '[': return make_token(TokenType::LeftBracket);
        case ']': return make_token(TokenType::RightBracket);
        case ',': return make_token(TokenType::Comma);
        case ':': return make_token(TokenType::Colon);
        case ';': return make_token(TokenType::Semicolon);
        case '+': return make_token(TokenType::Plus);
        case '*': return make_token(TokenType::Star);
        case '%': return make_token(TokenType::Percent);
        case '-': {
            if (match('>')) return make_token(TokenType::Arrow);
            return make_token(TokenType::Minus);
        }
        case '.':
            if (match('.')) {
                if (match('.')) return make_token(TokenType::DotDotDot);
                return make_token(TokenType::DotDot);
            }
            return make_token(TokenType::Dot);
        case '/':
            if (match('/')) {
                while (!is_at_end() && peek() != '\n') advance();
                return next_token();
            }
            return make_token(TokenType::Slash);
        case '=': return match('=') ? make_token(TokenType::EqualEqual) : make_token(TokenType::Equal);
        case '!': return match('=') ? make_token(TokenType::BangEqual) : make_token(TokenType::Bang);
        case '<':
            if (match('=')) return make_token(TokenType::LessEqual);
            if (match('<')) return make_token(TokenType::LessLess);
            return make_token(TokenType::Less);
        case '>':
            if (match('=')) return make_token(TokenType::GreaterEqual);
            if (match('>')) return make_token(TokenType::GreaterGreater);
            return make_token(TokenType::Greater);
        case '&': return match('&') ? make_token(TokenType::AmpAmp) : make_token(TokenType::Amp);
        case '|': return match('|') ? make_token(TokenType::PipePipe) : make_token(TokenType::Pipe);
        case '^': return make_token(TokenType::Caret);
        case '~': return make_token(TokenType::Tilde);
        case '@': return make_token(TokenType::At);
        default:
            return error_token("Unexpected character");
    }
}

Token Lexer::make_token(TokenType type) {
    return Token(type, source_.substr(start_, current_ - start_), line_, start_column_);
}

Token Lexer::make_token(TokenType type, const std::string& lexeme) {
    return Token(type, lexeme, line_, start_column_);
}

Token Lexer::error_token(const std::string& message) {
    return Token(TokenType::Error, message, line_, column_);
}

char Lexer::advance() {
    char c = source_[current_++];
    if (c == '\n') { line_++; column_ = 1; } else { column_++; }
    return c;
}

char Lexer::peek() const {
    if (is_at_end()) return '\0';
    return source_[current_];
}

char Lexer::peek_next() const {
    if (current_ + 1 >= (int)source_.size()) return '\0';
    return source_[current_ + 1];
}

bool Lexer::match(char expected) {
    if (is_at_end() || source_[current_] != expected) return false;
    current_++;
    column_++;
    return true;
}

bool Lexer::is_at_end() const {
    return current_ >= (int)source_.size();
}

void Lexer::skip_whitespace() {
    while (!is_at_end()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek_next() == '/') {
            // Line comment
            while (!is_at_end() && peek() != '\n') advance();
        } else {
            break;
        }
    }
}

Token Lexer::read_string() {
    char quote = source_[start_]; // " or '
    std::string value;
    while (!is_at_end() && peek() != quote) {
        if (peek() == '\\') {
            advance(); // consume backslash
            if (!is_at_end()) {
                char escaped = advance();
                switch (escaped) {
                    case 'n': value += '\n'; break;
                    case 't': value += '\t'; break;
                    case 'r': value += '\r'; break;
                    case '\\': value += '\\'; break;
                    case '\'': value += '\''; break;
                    case '"': value += '"'; break;
                    default: value += escaped; break;
                }
            }
        } else {
            value += advance();
        }
    }
    if (is_at_end()) return error_token("Unterminated string");
    advance(); // closing quote
    Token tok = make_token(TokenType::String);
    tok.string_value = value;
    return tok;
}

Token Lexer::read_number() {
    while (!is_at_end() && std::isdigit(peek())) advance();
    if (!is_at_end() && peek() == '.' && std::isdigit(peek_next())) {
        advance(); // consume '.'
        while (!is_at_end() && std::isdigit(peek())) advance();
    }
    Token tok = make_token(TokenType::Number);
    tok.number_value = std::strtod(source_.substr(start_, current_ - start_).c_str(), nullptr);
    return tok;
}

Token Lexer::read_identifier() {
    while (!is_at_end() && (std::isalnum(peek()) || peek() == '_')) advance();
    std::string text = source_.substr(start_, current_ - start_);
    TokenType type = identifier_type();
    Token tok = make_token(type, text);
    if (type == TokenType::True) tok.number_value = 1;
    if (type == TokenType::False) tok.number_value = 0;
    return tok;
}

TokenType Lexer::identifier_type() const {
    std::string text = source_.substr(start_, current_ - start_);
    if (text == "let") return TokenType::Let;
    if (text == "fn") return TokenType::Fn;
    if (text == "if") return TokenType::If;
    if (text == "else") return TokenType::Else;
    if (text == "while") return TokenType::While;
    if (text == "for") return TokenType::For;
    if (text == "in") return TokenType::In;
    if (text == "return") return TokenType::Return;
    if (text == "break") return TokenType::Break;
    if (text == "continue") return TokenType::Continue;
    if (text == "class") return TokenType::Class;
    if (text == "this") return TokenType::This;
    if (text == "super") return TokenType::Super;
    if (text == "new") return TokenType::New;
    if (text == "and") return TokenType::And;
    if (text == "or") return TokenType::Or;
    if (text == "not") return TokenType::Not;
    if (text == "include") return TokenType::Include;
    if (text == "await") return TokenType::Await;
    if (text == "switch") return TokenType::Switch;
    if (text == "case") return TokenType::Case;
    if (text == "default") return TokenType::Default;
    if (text == "try") return TokenType::Try;
    if (text == "catch") return TokenType::Catch;
    if (text == "throw") return TokenType::Throw;
    if (text == "signal") return TokenType::Signal;
    if (text == "effect") return TokenType::Effect;
    if (text == "enum") return TokenType::Enum;
    if (text == "true") return TokenType::True;
    if (text == "false") return TokenType::False;
    if (text == "nil") return TokenType::Nil;
    return TokenType::Identifier;
}

} // namespace akar
