#include "CodeGenerator.hpp"
#include <sstream> /* For building the string */
#include <set> /* For storing unique states */

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

std::string generate_dot_file(const std::string& machine_name, const std::vector<ParsedTransitionRule>& rules) {
    std::stringstream ss;

    // Start DOT file
    ss << "digraph " << machine_name << " {\n";
    ss << "    rankdir=LR;\n";  // Left to right layout
    ss << "    node [shape=box, style=filled, fillcolor=lightblue];\n";
    ss << "    edge [fontsize=10];\n\n";

    // Add all states
    std::set<std::string> states;
    for (const auto& rule : rules) {
        states.insert(rule.from_state_name);
        states.insert(rule.to_state_name);
    }

    // Add all transitions
    for (const auto& rule : rules) {
        ss << "    \"" << rule.from_state_name << "\" -> \"" << rule.to_state_name << "\" [label=\"";

        // Add predicates
        if (!rule.predicate_names.empty()) {
            ss << "PRED: ";
            for (size_t i = 0; i < rule.predicate_names.size(); ++i) {
                if (i > 0) ss << ", ";
                ss << rule.predicate_names[i];
            }
        }

        // Add actions
        if (!rule.action_names.empty()) {
            if (!rule.predicate_names.empty()) ss << "\\n";
            ss << "ACTION: ";
            for (size_t i = 0; i < rule.action_names.size(); ++i) {
                if (i > 0) ss << ", ";
                ss << rule.action_names[i];
            }
        }

        ss << "\"];\n";
    }

    ss << "}\n";
    return ss.str();
}

std::string generate_mermaid_file(const std::string& machine_name, const std::vector<ParsedTransitionRule>& rules) {
    std::stringstream ss;

    // Start Mermaid state diagram with title
    ss << "stateDiagram-v2\n";
    ss << "    title " << machine_name << "\n";
    ss << "    direction LR\n\n";  // Left to right layout

    // Add initial state transition
    for (const auto& rule : rules) {
        if (rule.from_state_name == "START") {
            ss << "    [*] --> " << rule.to_state_name;
            if (!rule.action_names.empty()) {
                ss << " : \"";
                for (size_t i = 0; i < rule.action_names.size(); ++i) {
                    if (i > 0) ss << ", ";
                    ss << rule.action_names[i] << "()";
                }
                ss << "\"";
            }
            ss << "\n";
            break;
        }
    }

    // Add all transitions
    for (const auto& rule : rules) {
        // Skip the initial state transition as we handled it above
        if (rule.from_state_name == "START") continue;

        ss << "    " << rule.from_state_name << " --> " << rule.to_state_name;

        // Add predicates and actions
        if (!rule.predicate_names.empty() || !rule.action_names.empty()) {
            ss << " : ";

            // Add predicates
            if (!rule.predicate_names.empty()) {
                if (rule.predicate_names.size() == 1) {
                    ss << "\"" << rule.predicate_names[0] << "()\"";
                } else {
                    ss << "[";
                    for (size_t i = 0; i < rule.predicate_names.size(); ++i) {
                        if (i > 0) ss << ", ";
                        ss << rule.predicate_names[i] << "()";
                    }
                    ss << "]";
                }
            }

            // Add actions
            if (!rule.action_names.empty()) {
                if (!rule.predicate_names.empty()) ss << " ";
                ss << "\"";
                for (size_t i = 0; i < rule.action_names.size(); ++i) {
                    if (i > 0) ss << ", ";
                    ss << rule.action_names[i] << "()";
                }
                ss << "\"";
            }
        }

        ss << "\n";
    }

    // Add final state transitions
    for (const auto& rule : rules) {
        if (rule.to_state_name == "DONE" || rule.to_state_name == "ERROR") {
            ss << "    " << rule.to_state_name << " --> [*]\n";
        }
    }

    return ss.str();
}

} /* namespace code_generator */
