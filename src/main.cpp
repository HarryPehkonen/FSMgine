#include "FSMgineCore.hpp"
#include <iostream>
#include <fstream> // Only if you want to test with file args, otherwise stdin/stdout

int main(int /* argc */, char** /* argv[] */) {
    // For simplicity, always use stdin and stdout as per the plan
    // You could add command-line argument parsing here to specify input/output files
    // if desired in the future.

    // Disable synchronization with C-style I/O for potentially faster std::cin/cout.
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    if (fsm_gine::process_source(std::cin, std::cout)) {
        return 0; // Success
    } else {
        std::cerr << "FSMgine: Processing failed.\n";
        return 1; // Failure
    }
}
