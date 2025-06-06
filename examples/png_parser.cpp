#include <string>
#include <vector>
#include <functional>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstdint>
#include <unordered_set>

// StringInterner for efficient string storage and comparison
class StringInterner {
public:
    static StringInterner& instance() {
        static StringInterner inst;
        return inst;
    }
    
    std::string_view intern(const std::string& str) {
        auto [it, inserted] = pool_.insert(str);
        return std::string_view(*it);
    }

private:
    std::unordered_set<std::string> pool_;
};

// Define your Transition struct
struct Transition {
    std::string_view from_state;
    std::vector<std::function<bool()>> predicates;
    std::vector<std::function<void()>> actions;
    std::string_view to_state;
};

/* FSMgine definition: PNGParser
(START ACTION readSignature CHECK_SIGNATURE)
(CHECK_SIGNATURE PRED isValidSignature ACTION printSignature READ_CHUNK)
(CHECK_SIGNATURE ACTION printError ERROR)
(READ_CHUNK ACTION readChunkLength PROCESS_CHUNK)
(PROCESS_CHUNK ACTION readChunkType CHECK_CHUNK_TYPE)
(CHECK_CHUNK_TYPE PRED isIHDR ACTION processIHDR READ_CHUNK)
(CHECK_CHUNK_TYPE PRED isPLTE ACTION processPLTE READ_CHUNK)
(CHECK_CHUNK_TYPE PRED isIDAT ACTION processIDAT READ_CHUNK)
(CHECK_CHUNK_TYPE PRED isIEND ACTION processIEND DONE)
(CHECK_CHUNK_TYPE ACTION processOtherChunk READ_CHUNK)
*/
#define FSM_PNGParser_transitions {}

class PNGParser {
public:
    PNGParser(const std::string& filename) : filename_(filename) {
        current_state_ = "START";
        transitions_ = FSM_PNGParser_transitions;
    }

    bool parse() {
        file_.open(filename_, std::ios::binary);
        if (!file_.is_open()) {
            std::cerr << "Failed to open file: " << filename_ << std::endl;
            return false;
        }

        while (current_state_ != "DONE" && current_state_ != "ERROR") {
            step();
        }

        file_.close();
        return current_state_ == "DONE";
    }

    void step() {
        for (const auto& rule : transitions_) {
            if (rule.from_state == current_state_) {
                bool all_preds_true = true;
                for (const auto& pred : rule.predicates) {
                    if (!pred()) {
                        all_preds_true = false;
                        break;
                    }
                }
                if (all_preds_true) {
                    for (const auto& action : rule.actions) {
                        action();
                    }
                    current_state_ = rule.to_state;
                    return;
                }
            }
        }
    }

private:
    // Predicates
    bool isValidSignature() {
        const uint8_t expected[8] = {137, 80, 78, 71, 13, 10, 26, 10};
        return std::equal(signature_, signature_ + 8, expected);
    }

    bool isIHDR() { return chunk_type_ == "IHDR"; }
    bool isPLTE() { return chunk_type_ == "PLTE"; }
    bool isIDAT() { return chunk_type_ == "IDAT"; }
    bool isIEND() { return chunk_type_ == "IEND"; }

    // Actions
    void readSignature() {
        file_.read(reinterpret_cast<char*>(signature_), 8);
    }

    void printSignature() {
        std::cout << "PNG Signature: ";
        for (int i = 0; i < 8; ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') 
                     << static_cast<int>(signature_[i]) << " ";
        }
        std::cout << std::dec << std::endl;
    }

    void printError() {
        std::cerr << "Invalid PNG signature!" << std::endl;
    }

    void readChunkLength() {
        file_.read(reinterpret_cast<char*>(&chunk_length_), 4);
        chunk_length_ = __builtin_bswap32(chunk_length_); // Convert from network byte order
    }

    void readChunkType() {
        char type[5] = {0};
        file_.read(type, 4);
        chunk_type_ = type;
    }

    void processIHDR() {
        std::cout << "\nIHDR Chunk:" << std::endl;
        uint32_t width, height;
        uint8_t bit_depth, color_type, compression, filter, interlace;
        
        file_.read(reinterpret_cast<char*>(&width), 4);
        file_.read(reinterpret_cast<char*>(&height), 4);
        file_.read(reinterpret_cast<char*>(&bit_depth), 1);
        file_.read(reinterpret_cast<char*>(&color_type), 1);
        file_.read(reinterpret_cast<char*>(&compression), 1);
        file_.read(reinterpret_cast<char*>(&filter), 1);
        file_.read(reinterpret_cast<char*>(&interlace), 1);

        width = __builtin_bswap32(width);
        height = __builtin_bswap32(height);

        std::cout << "  Width: " << width << std::endl;
        std::cout << "  Height: " << height << std::endl;
        std::cout << "  Bit Depth: " << static_cast<int>(bit_depth) << std::endl;
        std::cout << "  Color Type: " << static_cast<int>(color_type) << std::endl;
        std::cout << "  Compression: " << static_cast<int>(compression) << std::endl;
        std::cout << "  Filter: " << static_cast<int>(filter) << std::endl;
        std::cout << "  Interlace: " << static_cast<int>(interlace) << std::endl;

        // Skip CRC
        file_.seekg(4, std::ios::cur);
    }

    void processPLTE() {
        std::cout << "\nPLTE Chunk:" << std::endl;
        std::cout << "  Number of palette entries: " << chunk_length_ / 3 << std::endl;
        
        // Skip palette data and CRC
        file_.seekg(chunk_length_ + 4, std::ios::cur);
    }

    void processIDAT() {
        std::cout << "\nIDAT Chunk:" << std::endl;
        std::cout << "  Compressed data length: " << chunk_length_ << " bytes" << std::endl;
        
        // Skip IDAT data and CRC
        file_.seekg(chunk_length_ + 4, std::ios::cur);
    }

    void processIEND() {
        std::cout << "\nIEND Chunk (End of PNG file)" << std::endl;
        // Skip CRC
        file_.seekg(4, std::ios::cur);
    }

    void processOtherChunk() {
        std::cout << "\nChunk: " << chunk_type_ << std::endl;
        std::cout << "  Length: " << chunk_length_ << " bytes" << std::endl;
        
        // Skip chunk data and CRC
        file_.seekg(chunk_length_ + 4, std::ios::cur);
    }

    std::string filename_;
    std::ifstream file_;
    std::string_view current_state_;
    std::vector<Transition> transitions_;
    
    uint8_t signature_[8];
    uint32_t chunk_length_;
    std::string chunk_type_;
};

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <png_file>" << std::endl;
        return 1;
    }

    PNGParser parser(argv[1]);
    if (!parser.parse()) {
        return 1;
    }

    return 0;
} 