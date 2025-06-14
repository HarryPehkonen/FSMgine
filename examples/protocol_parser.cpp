#include <FSMgine/FSMgine.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>

namespace fsm = fsmgine;

// Protocol 1: Simple Command Protocol (SCP)
// Format: COMMAND:PARAM1=value1;PARAM2=value2;
class SCPParser {
public:
    struct SCPMessage {
        std::string command;
        std::map<std::string, std::string> params;
    };

    SCPParser() {
        setupFSM();
    }

    bool parse(const std::string& input) {
        reset();
        
        for (char c : input) {
            if (!fsm.process(c)) {
                std::cout << "Error parsing at character: " << c << std::endl;
                return false;
            }
        }
        
        // Process any remaining parameter
        if (!current_param.empty() && !current_value.empty()) {
            current_message.params[current_param] = current_value;
        }
        
        if (fsm.getCurrentState() != "END") {
            std::cout << "Incomplete message" << std::endl;
            return false;
        }
        
        printMessage();
        return true;
    }

private:
    fsm::FSM<char> fsm;
    SCPMessage current_message;
    std::string current_param;
    std::string current_value;

    void reset() {
        current_message = SCPMessage();
        current_param.clear();
        current_value.clear();
        fsm.setCurrentState("START");
    }

    void setupFSM() {
        auto builder = fsm.get_builder();
        
        // START → COMMAND
        builder.from("START")
            .predicate([](char c) { return isalpha(c); })
            .action([this](char c) { current_message.command += c; })
            .to("COMMAND");
        
        // COMMAND → COMMAND or PARAM_NAME
        builder.from("COMMAND")
            .predicate([](char c) { return isalnum(c); })
            .action([this](char c) { current_message.command += c; })
            .to("COMMAND");
        
        builder.from("COMMAND")
            .predicate([](char c) { return c == ':'; })
            .to("PARAM_NAME");
        
        // PARAM_NAME → PARAM_NAME or PARAM_VALUE
        builder.from("PARAM_NAME")
            .predicate([](char c) { return isalnum(c); })
            .action([this](char c) { current_param += c; })
            .to("PARAM_NAME");
        
        builder.from("PARAM_NAME")
            .predicate([](char c) { return c == '='; })
            .to("PARAM_VALUE");
        
        // PARAM_VALUE → PARAM_VALUE or PARAM_NAME or END
        builder.from("PARAM_VALUE")
            .predicate([](char c) { return c != ';'; })
            .action([this](char c) { current_value += c; })
            .to("PARAM_VALUE");
        
        builder.from("PARAM_VALUE")
            .predicate([](char c) { return c == ';'; })
            .action([this](char c) {
                current_message.params[current_param] = current_value;
                current_param.clear();
                current_value.clear();
            })
            .to("PARAM_NAME");
        
        // PARAM_NAME → END (when we see a semicolon with no param)
        builder.from("PARAM_NAME")
            .predicate([](char c) { return c == ';'; })
            .to("END");
    }

    void printMessage() {
        std::cout << "Command: " << current_message.command << std::endl;
        std::cout << "Parameters:" << std::endl;
        for (const auto& [key, value] : current_message.params) {
            std::cout << "  " << key << " = " << value << std::endl;
        }
    }
};

// Protocol 2: Simple Status Protocol (SSP)
// Format: STATUS[CODE]:MESSAGE
class SSPParser {
public:
    struct SSPMessage {
        std::string status;
        int code;
        std::string message;
    };

    SSPParser() {
        setupFSM();
    }

    bool parse(const std::string& input) {
        reset();
        
        for (char c : input) {
            if (!fsm.process(c)) {
                std::cout << "Error parsing at character: " << c << std::endl;
                return false;
            }
        }
        
        if (fsm.getCurrentState() != "END") {
            std::cout << "Incomplete message" << std::endl;
            return false;
        }
        
        printMessage();
        return true;
    }

private:
    fsm::FSM<char> fsm;
    SSPMessage current_message;
    std::string code_str;

    void reset() {
        current_message = SSPMessage();
        code_str.clear();
        fsm.setCurrentState("START");
    }

    void setupFSM() {
        auto builder = fsm.get_builder();
        
        // START → STATUS
        builder.from("START")
            .predicate([](char c) { return isalpha(c); })
            .action([this](char c) { current_message.status += c; })
            .to("STATUS");
        
        // STATUS → STATUS or CODE
        builder.from("STATUS")
            .predicate([](char c) { return isalpha(c); })
            .action([this](char c) { current_message.status += c; })
            .to("STATUS");
        
        builder.from("STATUS")
            .predicate([](char c) { return c == '['; })
            .to("CODE");
        
        // CODE → MESSAGE
        builder.from("CODE")
            .predicate([](char c) { return isdigit(c); })
            .action([this](char c) { code_str += c; })
            .to("CODE");
        
        builder.from("CODE")
            .predicate([](char c) { return c == ']'; })
            .action([this](char c) {
                current_message.code = std::stoi(code_str);
            })
            .to("COLON");
        
        // COLON → MESSAGE
        builder.from("COLON")
            .predicate([](char c) { return c == ':'; })
            .to("MESSAGE");
        
        // MESSAGE → MESSAGE or END
        builder.from("MESSAGE")
            .predicate([](char c) { return c != '\n'; })
            .action([this](char c) { current_message.message += c; })
            .to("MESSAGE");
        
        builder.from("MESSAGE")
            .predicate([](char c) { return c == '\n'; })
            .to("END");
    }

    void printMessage() {
        std::cout << "Status: " << current_message.status << std::endl;
        std::cout << "Code: " << current_message.code << std::endl;
        std::cout << "Message: " << current_message.message << std::endl;
    }
};

// Protocol 3: Simple Configuration Protocol (SCFP)
// Format: SECTION{KEY=value}
class SCFPParser {
public:
    struct SCFPMessage {
        std::string section;
        std::map<std::string, std::string> config;
    };

    SCFPParser() {
        setupFSM();
    }

    bool parse(const std::string& input) {
        reset();
        
        for (char c : input) {
            if (!fsm.process(c)) {
                std::cout << "Error parsing at character: " << c << std::endl;
                return false;
            }
        }
        
        if (fsm.getCurrentState() != "END") {
            std::cout << "Incomplete message" << std::endl;
            return false;
        }
        
        printMessage();
        return true;
    }

private:
    fsm::FSM<char> fsm;
    SCFPMessage current_message;
    std::string current_key;
    std::string current_value;

    void reset() {
        current_message = SCFPMessage();
        current_key.clear();
        current_value.clear();
        fsm.setCurrentState("START");
    }

    void setupFSM() {
        auto builder = fsm.get_builder();
        
        // START → SECTION
        builder.from("START")
            .predicate([](char c) { return isalpha(c); })
            .action([this](char c) { current_message.section += c; })
            .to("SECTION");
        
        // SECTION → SECTION or KEY
        builder.from("SECTION")
            .predicate([](char c) { return isalnum(c); })
            .action([this](char c) { current_message.section += c; })
            .to("SECTION");
        
        builder.from("SECTION")
            .predicate([](char c) { return c == '{'; })
            .to("KEY");
        
        // KEY → KEY or VALUE
        builder.from("KEY")
            .predicate([](char c) { return isalnum(c); })
            .action([this](char c) { current_key += c; })
            .to("KEY");
        
        builder.from("KEY")
            .predicate([](char c) { return c == '='; })
            .to("VALUE");
        
        // VALUE → VALUE or KEY or END
        builder.from("VALUE")
            .predicate([](char c) { return c != '}' && c != ','; })
            .action([this](char c) { current_value += c; })
            .to("VALUE");
        
        builder.from("VALUE")
            .predicate([](char c) { return c == ','; })
            .action([this](char c) {
                current_message.config[current_key] = current_value;
                current_key.clear();
                current_value.clear();
            })
            .to("KEY");
        
        builder.from("VALUE")
            .predicate([](char c) { return c == '}'; })
            .action([this](char c) {
                current_message.config[current_key] = current_value;
            })
            .to("END");
    }

    void printMessage() {
        std::cout << "Section: " << current_message.section << std::endl;
        std::cout << "Configuration:" << std::endl;
        for (const auto& [key, value] : current_message.config) {
            std::cout << "  " << key << " = " << value << std::endl;
        }
    }
};

int main() {
    // Test Simple Command Protocol
    std::cout << "Testing Simple Command Protocol (SCP):" << std::endl;
    SCPParser scp_parser;
    scp_parser.parse("SET:PARAM1=value1;PARAM2=value2;");
    std::cout << std::endl;

    // Test Simple Status Protocol
    std::cout << "Testing Simple Status Protocol (SSP):" << std::endl;
    SSPParser ssp_parser;
    ssp_parser.parse("SUCCESS[200]:Operation completed successfully\n");
    std::cout << std::endl;

    // Test Simple Configuration Protocol
    std::cout << "Testing Simple Configuration Protocol (SCFP):" << std::endl;
    SCFPParser scfp_parser;
    scfp_parser.parse("DATABASE{host=localhost,port=5432,user=admin}");
    
    return 0;
} 