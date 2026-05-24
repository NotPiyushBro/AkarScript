#pragma once
#include <string>
#include <vector>
#include <functional>

struct termios;

namespace akar {

class LineEditor {
public:
    // Completion callback: receives partial word, returns list of completions
    using CompletionFn = std::function<std::vector<std::string>(const std::string&)>;

    // history_path: empty = auto-detect (~/.akar_history)
    explicit LineEditor(const std::string& history_path = "");
    ~LineEditor();

    // Read a line with the given prompt. Returns false on EOF (Ctrl+D on empty line).
    bool readline(const std::string& prompt, std::string& result);

    // Manually add a line to history (called automatically by readline)
    void add_history(const std::string& line);

    // Save history to disk (called automatically by destructor)
    void save_history();

    // Set additional keywords for tab completion
    void set_keywords(std::vector<std::string> keywords);

    // Set custom completion callback (merged with keywords)
    void set_completion_callback(CompletionFn fn);

private:
    void enable_raw_mode();
    void disable_raw_mode();
    bool read_byte(unsigned char& c);
    void write_str(const char* s, size_t len);
    void write_str(const std::string& s);

    // Screen
    void redraw(const std::string& prompt);

    // Editing
    void insert_char(char c);
    void backspace();
    void delete_char();
    void move_left();
    void move_right();
    void move_home();
    void move_end();
    void delete_to_end();
    void delete_to_start();
    void delete_word();

    // History
    void history_prev();
    void history_next();
    void load_history();

    // Completion
    void do_tab(const std::string& prompt);
    std::vector<std::string> get_completions(const std::string& partial);

    // State
    std::string line_;
    size_t cursor_ = 0;
    std::vector<std::string> history_;
    size_t history_idx_ = 0;
    std::string saved_line_;  // line being edited when navigating history

    // Terminal
    bool is_tty_ = false;
    bool raw_mode_ = false;
    struct termios* orig_termios_ = nullptr;

    // Config
    std::string history_path_;
    std::vector<std::string> keywords_;
    CompletionFn completion_cb_;
};

} // namespace akar
