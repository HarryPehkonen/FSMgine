#ifndef FSMGINECORE_H
#define FSMGINECORE_H

#include <string>
#include <vector>
#include <iosfwd> // For istream, ostream forward declarations

namespace fsm_gine {

// Processes the input C++ source content and writes the modified content.
// Returns true on success, false on critical error during processing.
bool process_source(std::istream& input, std::ostream& output);

} // namespace fsm_gine

#endif // FSMGINECORE_H
