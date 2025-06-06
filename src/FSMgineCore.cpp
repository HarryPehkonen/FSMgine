#include "FSMgineCore.hpp"
#include "DSLRules.hpp"
#include "CodeGenerator.hpp"
#include <string>
#include <vector>
#include <iostream> // For std::cerr
#include <sstream>
#include <fstream>

namespace fsm_gine {

// Helper to extract machine name like "MyMachine1" from "/* FSMgine definition: MyMachine1"
std::string extract_fsm_name(std::string_view line) {
    const std::string_view start_marker = "/* FSMgine definition:";
    size_t pos = line.find(start_marker);
    if (pos == std::string_view::npos) {
        return ""; // Should not happen if called correctly
    }
    pos += start_marker.length();
    line.remove_prefix(pos); // Remove the marker part
    line = trim_view(line);  // Trim leading/trailing spaces from the rest

    // Name is the first word
    size_t name_end_pos = line.find_first_of(" \t\n\r");
    if (name_end_pos == std::string_view::npos) { // Name is the rest of the line
        return std::string(line);
    }
    return std::string(line.substr(0, name_end_pos));
}


bool process_source(std::istream& input, std::ostream& output, bool generate_dot, bool generate_mermaid) {
    std::string line;
    std::vector<std::string> output_buffer; // To reconstruct output

    enum class ParserState {
        SCANNING_FOR_DEFINITION,
        INSIDE_DEFINITION_BLOCK,
        SCANNING_FOR_MACRO_PLACEHOLDER
    };

    ParserState current_state = ParserState::SCANNING_FOR_DEFINITION;
    std::vector<std::string> current_dsl_content;
    std::string current_fsm_definition_name;

    while (std::getline(input, line)) {
        switch (current_state) {
            case ParserState::SCANNING_FOR_DEFINITION: {
                if (line.find("/* FSMgine definition:") != std::string::npos) {
                    current_fsm_definition_name = extract_fsm_name(line);
                    if (current_fsm_definition_name.empty()) {
                        std::cerr << "FSMgine Error: Could not parse FSM name from definition line: " << line << "\n";
                        // Continue scanning, but this block might be skipped
                    }
                    current_dsl_content.clear();
                    current_state = ParserState::INSIDE_DEFINITION_BLOCK;
                    if (!generate_dot && !generate_mermaid) {
                        output_buffer.push_back(line); // Keep the definition start comment
                    }
                } else if (!generate_dot && !generate_mermaid) {
                    output_buffer.push_back(line); // Pass through other lines
                }
                break;
            }

            case ParserState::INSIDE_DEFINITION_BLOCK: {
                if (!generate_dot && !generate_mermaid) {
                    output_buffer.push_back(line); // Keep all lines within the block for output integrity
                }
                if (line.find("*/") != std::string::npos) { // End of definition block
                    current_state = ParserState::SCANNING_FOR_MACRO_PLACEHOLDER;
                } else {
                    // Collect DSL lines, skipping FSMgine's own internal comments
                    std::string trimmed_line = std::string(trim_view(line)); // Convert to string for startswith
                    if (!trimmed_line.empty() && trimmed_line.rfind("//", 0) != 0 && trimmed_line.rfind("#", 0) != 0) {
                        current_dsl_content.push_back(trimmed_line);
                    }
                }
                break;
            }

            case ParserState::SCANNING_FOR_MACRO_PLACEHOLDER: {
                std::string trimmed_line = std::string(trim_view(line));
                std::string expected_macro_start = "#define FSM_" + current_fsm_definition_name + "_transitions";

                if (trimmed_line.rfind(expected_macro_start, 0) == 0 && // starts_with
                    trimmed_line.find("{}") != std::string::npos) {     // contains {}

                    std::vector<ParsedTransitionRule> parsed_rules;
                    bool all_rules_parsed_ok = true;
                    for (const auto& dsl_line : current_dsl_content) {
                        auto rule_opt = dsl_parser::parse_dsl_rule_line(dsl_line);
                        if (rule_opt) {
                            parsed_rules.push_back(*rule_opt);
                        } else {
                            std::cerr << "FSMgine Error: Failed to parse DSL rule. Skipping FSM '"
                                      << current_fsm_definition_name << "' generation.\n";
                            all_rules_parsed_ok = false;
                            break; // Stop parsing rules for this block
                        }
                    }

                    if (all_rules_parsed_ok && !parsed_rules.empty()) {
                        if (generate_dot) {
                            // Output DOT content directly to stdout
                            output << code_generator::generate_dot_file(current_fsm_definition_name, parsed_rules);
                        } else if (generate_mermaid) {
                            // Output Mermaid content directly to stdout
                            output << code_generator::generate_mermaid_file(current_fsm_definition_name, parsed_rules);
                        } else {
                            std::string generated_cpp = code_generator::generate_transitions_cpp(parsed_rules);

                            // Output the macro definition line with line continuation
                            output_buffer.push_back(expected_macro_start + " \\");

                            // Split generated_cpp into lines and append with indentation and line continuation
                            std::stringstream cpp_ss(generated_cpp);
                            std::string cpp_line;
                            std::vector<std::string> code_lines;
                            while(std::getline(cpp_ss, cpp_line, '\n')) {
                                code_lines.push_back(cpp_line);
                            }

                            for(size_t i = 0; i < code_lines.size(); ++i) {
                                std::string line_to_add = "    " + code_lines[i];
                                if (i < code_lines.size() - 1) { // Not the last line of generated code
                                    line_to_add += " \\";
                                }
                                output_buffer.push_back(line_to_add);
                            }
                        }
                    } else if (!generate_dot && !generate_mermaid) {
                        if (all_rules_parsed_ok && parsed_rules.empty() && !current_dsl_content.empty()) {
                            std::cerr << "FSMgine Warning: No valid transition rules found for FSM '"
                                      << current_fsm_definition_name << "'. Macro will be empty.\n";
                        }
                        output_buffer.push_back(line); // Output the original empty macro line
                    }
                    // Reset for the next potential FSM block
                    current_state = ParserState::SCANNING_FOR_DEFINITION;
                    current_dsl_content.clear();
                    current_fsm_definition_name.clear();
                } else if (!generate_dot && !generate_mermaid) {
                    output_buffer.push_back(line); // Pass through
                    // If we encounter another FSM definition block before finding the macro,
                    // it's a bit of a state error or implies the previous macro was missed.
                    if (line.find("/* FSMgine definition:") != std::string::npos) {
                        std::cerr << "FSMgine Warning: New FSM definition started before finding macro placeholder for '"
                                  << current_fsm_definition_name << "'. Previous DSL content discarded.\n";
                        // Effectively reset and re-process this line as a new definition start
                        current_fsm_definition_name = extract_fsm_name(line);
                        current_dsl_content.clear();
                        current_state = ParserState::INSIDE_DEFINITION_BLOCK;
                    }
                }
                break;
            }
        }
    }

    // After processing all input lines, write the buffered output (only in C++ generation mode)
    if (!generate_dot && !generate_mermaid) {
        for (const auto& out_line : output_buffer) {
            output << out_line << "\n";
        }
    }

    // Check if we ended in a state expecting more input (e.g. unclosed definition block)
    if (current_state == ParserState::INSIDE_DEFINITION_BLOCK) {
        std::cerr << "FSMgine Warning: Input ended while inside an FSM definition block for '" << current_fsm_definition_name << "'.\n";
    } else if (current_state == ParserState::SCANNING_FOR_MACRO_PLACEHOLDER) {
        std::cerr << "FSMgine Warning: Input ended while waiting for macro placeholder for FSM '" << current_fsm_definition_name << "'.\n";
    }

    return true; // For V1, always return true, errors are to stderr
}

} // namespace fsm_gine
