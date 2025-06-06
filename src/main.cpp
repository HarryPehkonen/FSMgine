#include "FSMgineCore.hpp"
#include <iostream>
#include <fstream> // Only if you want to test with file args, otherwise stdin/stdout
#include <string>
#include <vector>

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " [--generate-dot | --generate-mermaid]\n"
              << "  --generate-dot       Output GraphViz DOT file content\n"
              << "  --generate-mermaid   Output Mermaid diagram content\n"
              << "  -h, --help          Show this help message\n";
}

int main(int argc, char** argv) {
    enum class OutputFormat {
        CPP,
        DOT,
        MERMAID
    };
    OutputFormat format = OutputFormat::CPP;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--generate-dot") {
            format = OutputFormat::DOT;
        } else if (arg == "--generate-mermaid") {
            format = OutputFormat::MERMAID;
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    // Disable synchronization with C-style I/O for potentially faster std::cin/cout.
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    switch (format) {
        case OutputFormat::DOT:
            if (fsm_gine::process_source(std::cin, std::cout, true, false)) {
                return 0;
            }
            break;
        case OutputFormat::MERMAID:
            if (fsm_gine::process_source(std::cin, std::cout, false, true)) {
                return 0;
            }
            break;
        case OutputFormat::CPP:
            if (fsm_gine::process_source(std::cin, std::cout, false, false)) {
                return 0;
            }
            break;
    }

    std::cerr << "FSMgine: Processing failed.\n";
    return 1;
}
