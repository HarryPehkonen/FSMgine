#ifndef CODEGENERATOR_H
#define CODEGENERATOR_H

#include "DSLRules.hpp" // For ParsedTransitionRule
#include <string>
#include <vector>

namespace code_generator {

    // Generates the C++ code string for the body of the FSM_MachineName_transitions macro
    std::string generate_transitions_cpp(const std::vector<ParsedTransitionRule>& rules);

    // Generates a GraphViz DOT file representation of the state machine
    std::string generate_dot_file(const std::string& machine_name, const std::vector<ParsedTransitionRule>& rules);

    // Generates a Mermaid state diagram representation of the state machine
    std::string generate_mermaid_file(const std::string& machine_name, const std::vector<ParsedTransitionRule>& rules);

} // namespace code_generator

#endif // CODEGENERATOR_H
