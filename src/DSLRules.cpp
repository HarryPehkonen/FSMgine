#include "DSLRules.hpp"
#include <sstream>
#include <iostream> // For error messages
#include <algorithm> // For std::remove

// Helper to trim whitespace from both ends of a string_view
// C++17 doesn't have string_view::trim, so a bit manual or use a helper library
// For simplicity, we'll assume DSL lines are reasonably formatted or trimmed beforehand by the main loop.
// Or, we can implement basic trim here for string_view.
std::string_view trim_view(std::string_view sv) {
    const auto first = sv.find_first_not_of(" \t\n\r\f\v");
    if (std::string_view::npos == first) {
        return {};
    }
    const auto last = sv.find_last_not_of(" \t\n\r\f\v");
    return sv.substr(first, (last - first + 1));
}


namespace dsl_parser {

std::optional<ParsedTransitionRule> parse_dsl_rule_line(std::string_view line_sv) {
    line_sv = trim_view(line_sv);

    if (line_sv.empty() || line_sv.front() != '(' || line_sv.back() != ')') {
        std::cerr << "FSMgine DSL Error: Rule must be enclosed in parentheses. Line: '" << line_sv << "'\n";
        return std::nullopt;
    }

    // Remove parentheses for processing
    std::string_view content_sv = line_sv.substr(1, line_sv.length() - 2);
    content_sv = trim_view(content_sv);

    if (content_sv.empty()) {
        std::cerr << "FSMgine DSL Error: Empty rule content. Line: '" << line_sv << "'\n";
        return std::nullopt;
    }

    std::vector<std::string> tokens;
    std::string current_token_str;
    // Simple space-based tokenizer for string_view
    size_t start = 0;
    size_t end = 0;
    while ((end = content_sv.find(' ', start)) != std::string_view::npos) {
        if (end > start) { // Avoid empty tokens if multiple spaces
            tokens.push_back(std::string(content_sv.substr(start, end - start)));
        }
        start = end + 1;
    }
    if (start < content_sv.length()) { // Last token
         tokens.push_back(std::string(content_sv.substr(start)));
    }


    if (tokens.size() < 2) { // Must have at least FROM_STATE and TO_STATE
        std::cerr << "FSMgine DSL Error: Rule too short (needs at least FROM and TO states). Line: '" << line_sv << "' Got tokens: ";
        for(const auto& t : tokens)
		std::cerr << t << " ";
	std::cerr << "\n";
        return std::nullopt;
    }

    ParsedTransitionRule rule;
    rule.from_state_name = tokens.front();
    rule.to_state_name = tokens.back();

    // Process middle tokens for PRED and ACTION
    // Format: FROM_STATE [PRED name]* [ACTION name]* TO_STATE
    size_t current_token_idx = 1; // Start after FROM_STATE
    while (current_token_idx < tokens.size() - 1) { // Stop before TO_STATE
        const std::string& keyword_or_name = tokens[current_token_idx];

        if (keyword_or_name == "PRED") {
            if (current_token_idx + 1 < tokens.size() - 1) { // Check if a name follows
                rule.predicate_names.push_back(tokens[current_token_idx + 1]);
                current_token_idx += 2; // Consumed "PRED" and its name
            } else {
                std::cerr << "FSMgine DSL Error: 'PRED' keyword without a following name. Line: '" << line_sv << "'\n";
                return std::nullopt;
            }
        } else if (keyword_or_name == "ACTION") {
            if (current_token_idx + 1 < tokens.size() - 1) { // Check if a name follows
                rule.action_names.push_back(tokens[current_token_idx + 1]);
                current_token_idx += 2; // Consumed "ACTION" and its name
            } else {
                std::cerr << "FSMgine DSL Error: 'ACTION' keyword without a following name. Line: '" << line_sv << "'\n";
                return std::nullopt;
            }
        } else {
            std::cerr << "FSMgine DSL Error: Unexpected token '" << keyword_or_name
                      << "'. Expected 'PRED' or 'ACTION' keyword. Line: '" << line_sv << "'\n";
            return std::nullopt;
        }
    }
    return rule;
}

} // namespace dsl_parser
