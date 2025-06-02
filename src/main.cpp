#include "fsmgine.hpp"
#include <iostream> // For std::cerr, std::endl
#include <fstream>  // For std::ofstream to truncate files
#include <exception> // For std::exception

int main() {
    const std::string code_output_file = "code.cpp";
    const std::string header_output_file = "header.hpp";

    // Truncate/clear output files before starting
    std::ofstream(code_output_file, std::ios::trunc).close();
    std::ofstream(header_output_file, std::ios::trunc).close();
    // Could add checks here to ensure truncation was successful if needed

    try {
        // Create an FSMgine instance.
        // It now manages its own transitions and output files.
        FSMgine fsm(code_output_file, header_output_file);

        // Execute the FSM
        fsm.execute();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
