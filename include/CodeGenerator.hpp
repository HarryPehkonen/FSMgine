#ifndef CODEGENERATOR_H
#define CODEGENERATOR_H

#include "DSLRules.hpp" // For ParsedTransitionRule
#include <string>
#include <vector>

namespace code_generator {

    // Generates the C++ code string for the body of the FSM_MachineName_transitions macro
    std::string generate_transitions_cpp(const std::vector<ParsedTransitionRule>& rules);

} // namespace code_generator

#endif // CODEGENERATOR_H
