#include "CodeGenerator.hpp"
#include <sstream> /* For building the string */

namespace code_generator {

std::string generate_transitions_cpp(const std::vector<ParsedTransitionRule>& rules) {
    std::stringstream ss;
    ss << "std::vector<Transition>{\n"; /* Assuming 'Transition' struct is defined by the user */

    for (const auto& rule : rules) {
        ss << "    { /* Transition Rule from " << rule.from_state_name << " to " << rule.to_state_name << " */\n";
        ss << "      StringInterner::instance().intern(\"" << rule.from_state_name << "\"), /* from_state */\n";

        /* Predicates */
        ss << "      { /* Predicates */\n";
        if (rule.predicate_names.empty()) {
            /* No predicates */
        } else {
            for (const auto& pred_name : rule.predicate_names) {
                ss << "        [this]() { return this->" << pred_name << "(); },\n";
            }
        }
        ss << "      },\n";

        /* Actions */
        ss << "      { /* Actions */\n";
        if (rule.action_names.empty()) {
            /* No actions */
        } else {
            for (const auto& act_name : rule.action_names) {
                ss << "        [this]() { this->" << act_name << "(); },\n";
            }
        }
        ss << "      },\n";

        ss << "      StringInterner::instance().intern(\"" << rule.to_state_name << "\") /* to_state */\n";
        ss << "    }";
        /* Add a comma if it's not the last rule, or ensure the vector initializer is correct
         * For simplicity, always add a comma, then C++ compiler will deal with trailing comma in initializer list
         */
        ss << ",\n";
    }
    /* Clean up trailing comma if any for perfect syntax, though most compilers allow it in C++11+
     * A robust way would be to build a vector of strings then join with comma.
     * For now, the above is mostly fine.
     */

    ss << "}"; /* End of std::vector<Transition> */
    return ss.str();
}

} /* namespace code_generator */
