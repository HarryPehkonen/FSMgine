#pragma once

#include <optional>
#include <string>
#include <functional>
#include <vector>
#include <fstream> // For std::ofstream member

enum class State {
    IN_CODE,
    IN_HEADER,
    IN_BETWEEN,
    END
};

struct Transition {
    State start_state;
    std::function<bool(const std::optional<std::string>&)> predicate;
    std::function<void(const std::optional<std::string>&)> action;
    State new_state;
};

class FSMgine {
public:
    FSMgine(const std::string& code_filename, const std::string& header_filename);
    void execute();

private:
    State m_current_state_;
    std::vector<Transition> m_transitions_;

    std::ofstream m_code_file_stream_;
    std::ofstream m_header_file_stream_;

    // Initialization helper
    void initialize_transitions();

    // --- Internal FSM Logic Methods ---

    // Input method
    std::optional<std::string> m_get_input();

    // Predicates
    bool m_is_code_start(const std::optional<std::string>& s);
    bool m_is_header_start(const std::optional<std::string>& s);
    bool m_is_section_end(const std::optional<std::string>& s);
    bool m_is_eof(const std::optional<std::string>& s);
    bool m_always(const std::optional<std::string>& s);

    // Actions
    void m_handle_code_line(const std::optional<std::string>& s);
    void m_handle_header_line(const std::optional<std::string>& s);
    void m_discard_line(const std::optional<std::string>& s);
};
