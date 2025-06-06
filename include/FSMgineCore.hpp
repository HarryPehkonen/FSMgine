#ifndef FSMGINECORE_H
#define FSMGINECORE_H

#include <string>
#include <vector>
#include <iosfwd> // For istream, ostream forward declarations

namespace fsm_gine {

// Processes the input C++ source content and writes the modified content.
// If generate_dot is true, outputs DOT file content instead of C++ code.
// If generate_mermaid is true, outputs Mermaid diagram content instead of C++ code.
// Returns true on success, false on critical error during processing.
bool process_source(std::istream& input, std::ostream& output, bool generate_dot = false, bool generate_mermaid = false);

} // namespace fsm_gine

#endif // FSMGINECORE_H
