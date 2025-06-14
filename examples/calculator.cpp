#include <FSMgine/FSMgine.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <stack>
#include <cmath>
#include <sstream>

// Token types
enum class TokenType {
    NUMBER,
    PLUS,
    MINUS,
    MULTIPLY,
    DIVIDE,
    LPAREN,
    RPAREN,
    END
};

// Token structure
struct Token {
    TokenType type;
    double value;  // For NUMBER tokens
    std::string text;  // For debugging
    
    // Default constructor
    Token() : type(TokenType::END), value(0.0), text("") {}
    
    // Parameterized constructor
    Token(TokenType t, double v = 0.0, const std::string& txt = "") 
        : type(t), value(v), text(txt) {}
};

// Tokenizer FSM
class Tokenizer {
public:
    Tokenizer() : current_number(""), has_decimal(false) {}
    
    // State for building numbers
    std::string current_number;
    bool has_decimal;
    
    // Predicates
    bool is_digit(char c) const {
        return c >= '0' && c <= '9';
    }
    
    bool is_decimal(char c) const {
        return c == '.' && !has_decimal;
    }
    
    bool is_operator(char c) const {
        return c == '+' || c == '-' || c == '*' || c == '/';
    }
    
    bool is_paren(char c) const {
        return c == '(' || c == ')';
    }
    
    bool is_whitespace(char c) const {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }
    
    bool is_end(char c) const {
        return c == EOF;
    }
    
    // Actions
    void start_number(char c) {
        current_number = c;
        has_decimal = false;
    }
    
    void append_digit(char c) {
        current_number += c;
    }
    
    void add_decimal(char c) {
        current_number += c;
        has_decimal = true;
    }
    
    Token finish_number() {
        double value = std::stod(current_number);
        Token token(TokenType::NUMBER, value, current_number);
        current_number.clear();
        has_decimal = false;
        return token;
    }
    
    Token create_operator(char c) {
        TokenType type;
        switch(c) {
            case '+': type = TokenType::PLUS; break;
            case '-': type = TokenType::MINUS; break;
            case '*': type = TokenType::MULTIPLY; break;
            case '/': type = TokenType::DIVIDE; break;
            default: throw std::runtime_error("Invalid operator");
        }
        return Token(type, 0.0, std::string(1, c));
    }
    
    Token create_paren(char c) {
        TokenType type = (c == '(') ? TokenType::LPAREN : TokenType::RPAREN;
        return Token(type, 0.0, std::string(1, c));
    }
    
    Token create_end() {
        return Token(TokenType::END, 0.0, "END");
    }
};

// Parser FSM
class Parser {
public:
    Parser() = default;
    
    // Stacks for operator precedence parsing
    std::stack<TokenType> operators;
    std::stack<double> values;
    
    // Predicates
    bool is_number(const Token& t) const {
        return t.type == TokenType::NUMBER;
    }
    
    bool is_operator(const Token& t) const {
        return t.type == TokenType::PLUS || t.type == TokenType::MINUS ||
               t.type == TokenType::MULTIPLY || t.type == TokenType::DIVIDE;
    }
    
    bool is_lparen(const Token& t) const {
        return t.type == TokenType::LPAREN;
    }
    
    bool is_rparen(const Token& t) const {
        return t.type == TokenType::RPAREN;
    }
    
    bool is_end(const Token& t) const {
        return t.type == TokenType::END;
    }
    
    // Actions
    void push_number(const Token& t) {
        values.push(t.value);
    }
    
    void push_operator(const Token& t) {
        while (!operators.empty() && 
               precedence(operators.top()) >= precedence(t.type)) {
            evaluate_top();
        }
        operators.push(t.type);
    }
    
    void push_lparen(const Token& t) {
        operators.push(t.type);
    }
    
    void handle_rparen(const Token& t) {
        while (!operators.empty() && operators.top() != TokenType::LPAREN) {
            evaluate_top();
        }
        if (!operators.empty()) {
            operators.pop();  // Pop the LPAREN
        }
    }
    
    void finish_parsing(const Token& t) {
        while (!operators.empty()) {
            evaluate_top();
        }
        if (!values.empty()) {
            std::cout << "Result: " << values.top() << std::endl;
        }
    }
    
private:
    int precedence(TokenType op) const {
        switch(op) {
            case TokenType::MULTIPLY:
            case TokenType::DIVIDE:
                return 2;
            case TokenType::PLUS:
            case TokenType::MINUS:
                return 1;
            default:
                return 0;
        }
    }
    
    void evaluate_top() {
        if (values.size() < 2 || operators.empty()) return;
        
        double b = values.top(); values.pop();
        double a = values.top(); values.pop();
        TokenType op = operators.top(); operators.pop();
        
        double result;
        switch(op) {
            case TokenType::PLUS: result = a + b; break;
            case TokenType::MINUS: result = a - b; break;
            case TokenType::MULTIPLY: result = a * b; break;
            case TokenType::DIVIDE: 
                if (b == 0) throw std::runtime_error("Division by zero");
                result = a / b; 
                break;
            default: throw std::runtime_error("Invalid operator");
        }
        values.push(result);
    }
};

int main() {
    // Create tokenizer FSM
    fsmgine::FSM<char> tokenizer;
    Tokenizer tokenizer_state;
    std::vector<Token> tokens;
    
    // Build tokenizer FSM
    tokenizer.get_builder()
        .from("START")
        .predicate([&tokenizer_state](char c) { return tokenizer_state.is_digit(c); })
        .action([&tokenizer_state](char c) { 
            if (tokenizer_state.current_number.empty()) {
                tokenizer_state.start_number(c);
            } else {
                tokenizer_state.append_digit(c);
            }
        })
        .to("NUMBER");

    tokenizer.get_builder()
        .from("NUMBER")
        .predicate([&tokenizer_state](char c) { return tokenizer_state.is_digit(c); })
        .action([&tokenizer_state](char c) { tokenizer_state.append_digit(c); })
        .to("NUMBER");

    tokenizer.get_builder()
        .from("NUMBER")
        .predicate([&tokenizer_state](char c) { return tokenizer_state.is_decimal(c); })
        .action([&tokenizer_state](char c) { tokenizer_state.add_decimal(c); })
        .to("NUMBER");

    tokenizer.get_builder()
        .from("NUMBER")
        .predicate([&tokenizer_state](char c) { 
            return tokenizer_state.is_operator(c) || tokenizer_state.is_paren(c) || 
                   tokenizer_state.is_whitespace(c) || c == '\n' || tokenizer_state.is_end(c); 
        })
        .action([&tokenizer_state, &tokens](char c) {
            tokens.push_back(tokenizer_state.finish_number());
            if (!tokenizer_state.is_whitespace(c) && c != '\n' && !tokenizer_state.is_end(c)) {
                if (tokenizer_state.is_operator(c)) {
                    tokens.push_back(tokenizer_state.create_operator(c));
                } else {
                    tokens.push_back(tokenizer_state.create_paren(c));
                }
            }
        })
        .to("START");

    tokenizer.get_builder()
        .from("START")
        .predicate([&tokenizer_state](char c) { return tokenizer_state.is_operator(c); })
        .action([&tokenizer_state, &tokens](char c) { 
            tokens.push_back(tokenizer_state.create_operator(c)); 
        })
        .to("START");

    tokenizer.get_builder()
        .from("START")
        .predicate([&tokenizer_state](char c) { return tokenizer_state.is_paren(c); })
        .action([&tokenizer_state, &tokens](char c) { 
            tokens.push_back(tokenizer_state.create_paren(c)); 
        })
        .to("START");

    tokenizer.get_builder()
        .from("START")
        .predicate([&tokenizer_state](char c) { return c == '\n' || tokenizer_state.is_end(c); })
        .action([&tokenizer_state, &tokens](char c) { 
            if (!tokenizer_state.current_number.empty()) {
                tokens.push_back(tokenizer_state.finish_number());
            }
            tokens.push_back(tokenizer_state.create_end()); 
        })
        .to("END");

    // Create parser FSM
    fsmgine::FSM<Token> parser;
    Parser parser_state;
    
    // Build parser FSM
    parser.get_builder()
        .from("START")
        .predicate([&parser_state](const Token& t) { return parser_state.is_number(t); })
        .action([&parser_state](const Token& t) { parser_state.push_number(t); })
        .to("START");

    parser.get_builder()
        .from("START")
        .predicate([&parser_state](const Token& t) { return parser_state.is_operator(t); })
        .action([&parser_state](const Token& t) { parser_state.push_operator(t); })
        .to("START");

    parser.get_builder()
        .from("START")
        .predicate([&parser_state](const Token& t) { return parser_state.is_lparen(t); })
        .action([&parser_state](const Token& t) { parser_state.push_lparen(t); })
        .to("START");

    parser.get_builder()
        .from("START")
        .predicate([&parser_state](const Token& t) { return parser_state.is_rparen(t); })
        .action([&parser_state](const Token& t) { parser_state.handle_rparen(t); })
        .to("START");

    parser.get_builder()
        .from("START")
        .predicate([&parser_state](const Token& t) { return parser_state.is_end(t); })
        .action([&parser_state](const Token& t) { parser_state.finish_parsing(t); })
        .to("END");

    // Set initial states
    tokenizer.setInitialState("START");
    parser.setInitialState("START");
    
    // Run the calculator
    std::cout << "Simple Calculator\n";
    std::cout << "Enter expressions (Ctrl+D to exit):\n";
    
    while (true) {
        // Reset state for new expression
        tokens.clear();
        tokenizer.setInitialState("START");
        parser.setInitialState("START");
        
        // First phase: tokenization
        while (tokenizer.getCurrentState() != "END") {
            char c;
            if (!std::cin.get(c)) {
                return 0;  // Exit on EOF
            }
            tokenizer.process(c);
        }
        
        // Second phase: parsing and evaluation
        for (const auto& token : tokens) {
            parser.process(token);
        }
        
        std::cout << "> ";  // Prompt for next expression
    }
    
    return 0;
} 