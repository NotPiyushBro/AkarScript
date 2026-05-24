#include "akar/util/repl.h"

#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace akar {

static constexpr size_t MAX_HISTORY = 500;
static constexpr int ESCAPE_TIMEOUT_US = 50000; // 50ms

// Default Akar keywords for tab completion
static const char* DEFAULT_KEYWORDS[] = {
    "let", "fn", "class", "if", "else", "while", "for", "in",
    "return", "true", "false", "nil", "and", "or", "not",
    "this", "super", "print", "break", "continue",
    "switch", "case", "default", "try", "catch", "throw",
    "signal", "effect", "enum", "include",
    nullptr
};

// Akar builtins for tab completion
static const char* DEFAULT_BUILTINS[] = {
    "len", "type", "to_string", "to_number", "to_int",
    "format", "assert", "exit", "sleep", "time", "clock",
    "push", "pop", "shift", "unshift", "contains",
    "join", "split", "trim", "replace", "sub",
    "starts_with", "ends_with", "find", "char_at",
    "lower", "upper", "reverse", "sort",
    "floor", "ceil", "round", "abs", "sqrt",
    "sin", "cos", "tan", "log", "pow", "exp",
    "random", "random_seed", "min", "max", "clamp",
    "fiber_create", "fiber_resume", "fiber_yield", "fiber_status",
    "trace_start", "trace_stop", "trace_log",
    "profile_start", "profile_stop", "profile_report",
    "vec2", "vec3", "vec_len", "vec_dot", "vec_normalize",
    // System libs
    "io", "fs", "path", "sys", "net",
    nullptr
};

static std::string default_history_path() {
    const char* home = std::getenv("HOME");
    if (home) return std::string(home) + "/.akar_history";
    return ".akar_history";
}

LineEditor::LineEditor(const std::string& history_path)
    : history_path_(history_path.empty() ? default_history_path() : history_path),
      orig_termios_(new struct termios)
{
    is_tty_ = isatty(STDIN_FILENO) != 0;

    // Populate default keywords
    for (const char** kw = DEFAULT_KEYWORDS; *kw; ++kw)
        keywords_.emplace_back(*kw);
    for (const char** kw = DEFAULT_BUILTINS; *kw; ++kw)
        keywords_.emplace_back(*kw);

    load_history();
}

LineEditor::~LineEditor() {
    save_history();
    if (raw_mode_) disable_raw_mode();
    delete orig_termios_;
}

// ─── Terminal raw mode ──────────────────────────────────────────

void LineEditor::enable_raw_mode() {
    if (!is_tty_ || raw_mode_) return;
    if (tcgetattr(STDIN_FILENO, orig_termios_) == -1) return;

    struct termios raw = *orig_termios_;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) return;
    raw_mode_ = true;
}

void LineEditor::disable_raw_mode() {
    if (!raw_mode_) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, orig_termios_);
    raw_mode_ = false;
}

// ─── Low-level I/O ─────────────────────────────────────────────

bool LineEditor::read_byte(unsigned char& c) {
    return read(STDIN_FILENO, &c, 1) == 1;
}

void LineEditor::write_str(const char* s, size_t len) {
    (void)write(STDOUT_FILENO, s, len);
}

void LineEditor::write_str(const std::string& s) {
    write_str(s.data(), s.size());
}

// ─── Screen redraw ─────────────────────────────────────────────

void LineEditor::redraw(const std::string& prompt) {
    // \r = carriage return (col 0), \x1b[K = erase to end of line
    std::string buf;
    buf.reserve(prompt.size() + line_.size() + 32);
    buf += "\r\x1b[K";
    buf += prompt;
    buf += line_;
    write_str(buf);

    // Move cursor back if not at end of line
    if (cursor_ < line_.size()) {
        size_t back = line_.size() - cursor_;
        buf = "\x1b[" + std::to_string(back) + "D";
        write_str(buf);
    }
}

// ─── Editing operations ────────────────────────────────────────

void LineEditor::insert_char(char c) {
    line_.insert(line_.begin() + cursor_, c);
    ++cursor_;
}

void LineEditor::backspace() {
    if (cursor_ > 0) {
        line_.erase(line_.begin() + (--cursor_));
    }
}

void LineEditor::delete_char() {
    if (cursor_ < line_.size()) {
        line_.erase(line_.begin() + cursor_);
    }
}

void LineEditor::move_left() {
    if (cursor_ > 0) --cursor_;
}

void LineEditor::move_right() {
    if (cursor_ < line_.size()) ++cursor_;
}

void LineEditor::move_home() {
    cursor_ = 0;
}

void LineEditor::move_end() {
    cursor_ = line_.size();
}

void LineEditor::delete_to_end() {
    line_.resize(cursor_);
}

void LineEditor::delete_to_start() {
    line_.erase(0, cursor_);
    cursor_ = 0;
}

void LineEditor::delete_word() {
    if (cursor_ == 0) return;
    size_t end = cursor_;
    size_t start = end;
    // Skip trailing spaces
    while (start > 0 && line_[start - 1] == ' ') --start;
    // Skip word characters
    while (start > 0 && line_[start - 1] != ' ') --start;
    line_.erase(start, end - start);
    cursor_ = start;
}

// ─── History ───────────────────────────────────────────────────

void LineEditor::history_prev() {
    if (history_.empty() || history_idx_ == 0) return;
    if (history_idx_ == history_.size()) {
        saved_line_ = line_; // Save what user was typing
    }
    --history_idx_;
    line_ = history_[history_idx_];
    cursor_ = line_.size();
}

void LineEditor::history_next() {
    if (history_.empty() || history_idx_ >= history_.size()) return;
    ++history_idx_;
    if (history_idx_ == history_.size()) {
        line_ = saved_line_;
    } else {
        line_ = history_[history_idx_];
    }
    cursor_ = line_.size();
}

void LineEditor::add_history(const std::string& line) {
    if (line.empty()) return;
    // Don't add duplicate of last entry
    if (!history_.empty() && history_.back() == line) return;
    history_.push_back(line);
    if (history_.size() > MAX_HISTORY) {
        history_.erase(history_.begin());
    }
}

void LineEditor::load_history() {
    std::ifstream file(history_path_);
    if (!file.is_open()) return;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) history_.push_back(line);
    }
    // Trim to max size
    if (history_.size() > MAX_HISTORY) {
        history_.erase(history_.begin(),
                       history_.begin() + (history_.size() - MAX_HISTORY));
    }
}

void LineEditor::save_history() {
    std::ofstream file(history_path_, std::ios::trunc);
    if (!file.is_open()) return;
    for (const auto& entry : history_) {
        file << entry << '\n';
    }
}

// ─── Tab completion ────────────────────────────────────────────

std::vector<std::string> LineEditor::get_completions(const std::string& partial) {
    std::vector<std::string> matches;

    // Search built-in keywords
    for (const auto& kw : keywords_) {
        if (kw.size() >= partial.size() &&
            kw.compare(0, partial.size(), partial) == 0) {
            matches.push_back(kw);
        }
    }

    // Search custom completions
    if (completion_cb_) {
        auto extra = completion_cb_(partial);
        for (auto& e : extra) {
            if (e.size() >= partial.size() &&
                e.compare(0, partial.size(), partial) == 0) {
                matches.push_back(std::move(e));
            }
        }
    }

    // Deduplicate
    std::sort(matches.begin(), matches.end());
    matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
    return matches;
}

void LineEditor::do_tab(const std::string& prompt) {
    // Find the word being typed (scan backwards from cursor to space/paren/etc.)
    size_t word_start = cursor_;
    while (word_start > 0) {
        char c = line_[word_start - 1];
        if (c == ' ' || c == '(' || c == ')' || c == '{' || c == '}' ||
            c == '[' || c == ']' || c == ',' || c == '.' || c == '=' ||
            c == '+' || c == '-' || c == '*' || c == '/' || c == '!' ||
            c == '<' || c == '>' || c == '&' || c == '|' || c == '\t')
            break;
        --word_start;
    }
    std::string partial = line_.substr(word_start, cursor_ - word_start);
    if (partial.empty()) return;

    auto matches = get_completions(partial);
    if (matches.empty()) return;

    if (matches.size() == 1) {
        // Single match: complete it
        std::string suffix = matches[0].substr(partial.size());
        line_.insert(cursor_, suffix);
        cursor_ += suffix.size();
    } else {
        // Multiple matches: find common prefix
        std::string common = matches[0];
        for (size_t i = 1; i < matches.size(); ++i) {
            size_t j = 0;
            while (j < common.size() && j < matches[i].size() &&
                   common[j] == matches[i][j])
                ++j;
            common.resize(j);
        }
        // Apply common prefix if longer than what user typed
        if (common.size() > partial.size()) {
            std::string suffix = common.substr(partial.size());
            line_.insert(cursor_, suffix);
            cursor_ += suffix.size();
        }

        // Show completions below the current line
        std::string msg = "\n";
        for (const auto& m : matches) {
            msg += "  " + m;
        }
        msg += "\n";
        write_str(msg);
    }
}

// ─── Keywords / completion callback ────────────────────────────

void LineEditor::set_keywords(std::vector<std::string> keywords) {
    keywords_ = std::move(keywords);
}

void LineEditor::set_completion_callback(CompletionFn fn) {
    completion_cb_ = std::move(fn);
}

// ─── Main readline ─────────────────────────────────────────────

bool LineEditor::readline(const std::string& prompt, std::string& result) {
    if (!is_tty_) {
        // Non-interactive: fall back to std::getline
        std::cout << prompt << std::flush;
        if (!std::getline(std::cin, result)) return false;
        return true;
    }

    line_.clear();
    cursor_ = 0;
    history_idx_ = history_.size();
    saved_line_.clear();

    enable_raw_mode();

    // Write the initial prompt
    write_str(prompt);

    while (true) {
        unsigned char c;
        if (!read_byte(c)) {
            disable_raw_mode();
            return false;
        }

        // ── Escape sequences ──
        if (c == 0x1b) {
            unsigned char seq[4];
            fd_set fds;
            struct timeval tv;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);
            tv.tv_sec = 0;
            tv.tv_usec = ESCAPE_TIMEOUT_US;

            if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0) {
                if (!read_byte(seq[0])) { redraw(prompt); continue; }

                if (seq[0] == '[') {
                    if (!read_byte(seq[1])) { redraw(prompt); continue; }
                    switch (seq[1]) {
                    case 'A': history_prev(); break;        // ↑
                    case 'B': history_next(); break;        // ↓
                    case 'C': move_right(); break;          // →
                    case 'D': move_left(); break;           // ←
                    case 'H': move_home(); break;           // Home
                    case 'F': move_end(); break;            // End
                    case '3':                               // Delete
                        if (read_byte(seq[2]) && seq[2] == '~')
                            delete_char();
                        break;
                    case '1': case '2': case '5': case '6':
                        // Extended sequences (Ctrl+arrows, Shift+arrows, etc.)
                        // Consume until final letter
                        {
                            unsigned char tmp;
                            while (read_byte(tmp)) {
                                if (tmp == '~' || (tmp >= 'A' && tmp <= 'Z'))
                                    break;
                            }
                        }
                        break;
                    default:
                        break;
                    }
                }
                // else: Alt+<key> — ignore
            }
            // else: bare Escape — ignore
            redraw(prompt);
            continue;
        }

        // ── Control characters ──
        switch (c) {
        case '\r': case '\n': {         // Enter
            disable_raw_mode();
            write_str("\n");
            result = line_;
            if (!result.empty()) add_history(result);
            return true;
        }
        case '\t': {                    // Tab
            do_tab(prompt);
            redraw(prompt);
            continue;
        }
        case 3: {                       // Ctrl+C
            disable_raw_mode();
            write_str("^C\n");
            line_.clear();
            cursor_ = 0;
            result.clear();
            return true;  // Caller gets empty string
        }
        case 4: {                       // Ctrl+D
            if (line_.empty()) {
                disable_raw_mode();
                write_str("\n");
                return false;  // EOF
            }
            delete_char();
            break;
        }
        case 127: case '\b': {          // Backspace (DEL or BS)
            backspace();
            break;
        }
        case 1:   move_home(); break;   // Ctrl+A
        case 5:   move_end(); break;    // Ctrl+E
        case 11:  delete_to_end(); break;   // Ctrl+K
        case 21:  delete_to_start(); break; // Ctrl+U
        case 23:  delete_word(); break;     // Ctrl+W
        case 12:  break;                // Ctrl+L — ignore (could clear screen)
        default:
            if (c >= 32) {
                insert_char(static_cast<char>(c));
            }
            break;
        }

        redraw(prompt);
    }
}

} // namespace akar
