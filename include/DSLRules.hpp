#ifndef DSLRULES_H
#define DSLRULES_H

#include <string>
#include <vector>
#include <optional>
#include <string_view>

std::string_view trim_view(std::string_view sv);

// Holds the parsed information from a single DSL transition rule
struct ParsedTransitionRule {
    std::string from_state_name;
    std::vector<std::string> predicate_names;
    std::vector<std::string> action_names;
    std::string to_state_name;
};

namespace dsl_parser {
    // Parses a single line of the FSM DSL.
    // Example line: "(S1 PRED p1 ACTION a1 TO_S2)"
    // Returns std::nullopt on parsing error, with an error message to stderr.
    std::optional<ParsedTransitionRule> parse_dsl_rule_line(std::string_view line);

} // namespace dsl_parser

#endif // DSLRULES_H
