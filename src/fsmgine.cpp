#include "fsmgine.hpp"

#include <iostream>  // For std::cin, std::getline, std::endl
#include <ranges>    // For std::ranges::views::filter, std::ranges::find_if
#include <algorithm> // For std::ranges::find_if (often in <algorithm> though primarily <ranges>)
#include <stdexcept> // For std::runtime_error

FSMgine::FSMgine(const std::string& code_filename, const std::string& header_filename)
    : m_current_state_(State::IN_BETWEEN) {

    // Open files in append mode. Truncation should happen before FSMgine is created
    // or at the start of execute if preferred.
    m_code_file_stream_.open(code_filename, std::ios::app);
    m_header_file_stream_.open(header_filename, std::ios::app);

    if (!m_code_file_stream_.is_open()) {
        throw std::runtime_error("Failed to open code output file: " + code_filename);
    }
    if (!m_header_file_stream_.is_open()) {
        throw std::runtime_error("Failed to open header output file: " + header_filename);
    }

    initialize_transitions();
}

void FSMgine::initialize_transitions() {

    // The lambdas capture 'this' to call the member functions.
    // This is where a generator would produce code to define the transitions.
    m_transitions_ = {

        // Transitions from IN_BETWEEN state
        {State::IN_BETWEEN, [this](const auto& s){ return this->m_is_code_start(s); },    [this](const auto& s){ this->m_discard_line(s); },        State::IN_CODE},
        {State::IN_BETWEEN, [this](const auto& s){ return this->m_is_header_start(s); },  [this](const auto& s){ this->m_discard_line(s); },        State::IN_HEADER},
        {State::IN_BETWEEN, [this](const auto& s){ return this->m_is_eof(s); },           [this](const auto& s){ this->m_discard_line(s); },        State::END},
        {State::IN_BETWEEN, [this](const auto& s){ return this->m_always(s); },           [this](const auto& s){ this->m_discard_line(s); },        State::IN_BETWEEN},

        // Transitions from IN_CODE state
        {State::IN_CODE,    [this](const auto& s){ return this->m_is_section_end(s); }, [this](const auto& s){ this->m_discard_line(s); },        State::IN_BETWEEN},
        {State::IN_CODE,    [this](const auto& s){ return this->m_always(s); },         [this](const auto& s){ this->m_handle_code_line(s); },   State::IN_CODE},

        // Transitions from IN_HEADER state
        {State::IN_HEADER,  [this](const auto& s){ return this->m_is_section_end(s); }, [this](const auto& s){ this->m_discard_line(s); },        State::IN_BETWEEN},
        {State::IN_HEADER,  [this](const auto& s){ return this->m_always(s); },         [this](const auto& s){ this->m_handle_header_line(s); }, State::IN_HEADER}
    };
}

// --- Internal FSM Logic Methods ---

std::optional<std::string> FSMgine::m_get_input() {
    std::string line;
    if (std::getline(std::cin, line)) {
        return std::optional<std::string>(line);
    }
    return std::nullopt; // EOF or error
}

// Predicates
bool FSMgine::m_is_code_start(const std::optional<std::string>& s) {
    return s.has_value() && s.value() == "code %{";
}

bool FSMgine::m_is_header_start(const std::optional<std::string>& s) {
    return s.has_value() && s.value() == "header %{";
}

bool FSMgine::m_is_section_end(const std::optional<std::string>& s) {
    return s.has_value() && s.value() == "%}";
}

bool FSMgine::m_is_eof(const std::optional<std::string>& s) {
    return !s.has_value();
}

bool FSMgine::m_always(const std::optional<std::string>& /*s*/) {
    return true;
}

// Actions
void FSMgine::m_handle_code_line(const std::optional<std::string>& s_opt) {
    if (s_opt.has_value() && m_code_file_stream_.is_open()) {
        m_code_file_stream_ << s_opt.value() << std::endl;
    }
}

void FSMgine::m_handle_header_line(const std::optional<std::string>& s_opt) {
    if (s_opt.has_value() && m_header_file_stream_.is_open()) {
        m_header_file_stream_ << s_opt.value() << std::endl;
    }
}

void FSMgine::m_discard_line(const std::optional<std::string>& /*s_opt*/) {
    // Do nothing
}

// --- Public FSM Execution ---

void FSMgine::execute() {
    while (m_current_state_ != State::END) {
        std::optional<std::string> input_line = m_get_input(); // Read input ONCE per cycle

        auto transitions_for_current_state = std::ranges::views::filter(
            m_transitions_,
            [this](const Transition& t) {
                return t.start_state == this->m_current_state_;
            }
        );

        auto it = std::ranges::find_if(
            transitions_for_current_state,
            [&input_line](const Transition& t) { // Pass the stored input_line
                return t.predicate(input_line);
            }
        );

        if (it == transitions_for_current_state.end()) {
            std::string err_msg = "No suitable transition found for state ";
            // Consider adding a state to string conversion for better error messages
            err_msg += std::to_string(static_cast<int>(m_current_state_));
            err_msg += " and input: " + (input_line.has_value() ? "'" + input_line.value() + "'" : "EOF");
            throw std::runtime_error(err_msg);
        }

        it->action(input_line); // Execute action with the same input_line
        m_current_state_ = it->new_state;
    }
    // Streams will be closed when FSMgine object is destructed.
}