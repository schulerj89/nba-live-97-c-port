#ifndef NBA97_GAMELOAD_ENTRY_CAPTURE_H
#define NBA97_GAMELOAD_ENTRY_CAPTURE_H

#include <string>

namespace nba97 {

/* Deterministic standalone GAMELOAD-entry evidence. Large journals are
 * represented by documented canonical FNV-1a hashes plus exact counts and
 * boundary samples; both child-entry machine snapshots remain explicit. */
std::string captureGameloadEntry();

} // namespace nba97
#endif
