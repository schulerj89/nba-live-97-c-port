#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace nba97 {
using RosterSlots = std::array<std::uint16_t,535>;
using RosterBaseIdentity = std::array<std::uint8_t,32>;
enum class RosterSaveErrorKind { Corrupt, Unsupported, WrongBase, InvalidRoster };
class RosterSaveError : public std::runtime_error {
public:
    RosterSaveError(RosterSaveErrorKind kind,const char* message) : std::runtime_error(message),kind_(kind) {}
    RosterSaveErrorKind kind() const noexcept { return kind_; }
private: RosterSaveErrorKind kind_;
};
struct RosterSaveExtension {
    std::array<char,4> tag{};
    std::uint16_t version=1;
    std::vector<std::uint8_t> payload;
};
struct RosterSaveDocument {
    std::uint64_t generation=0;
    std::uint16_t minor_version=0;
    RosterSlots slots{};
    // Only unknown OPTIONAL sections reach this vector. Required unknown
    // sections fail closed; rewriting must preserve optional bytes/version.
    std::vector<RosterSaveExtension> extensions;
};
constexpr std::size_t kRosterSaveMaxBytes=1024*1024;
constexpr unsigned kRosterSaveMaxSections=64;
std::uint32_t rosterSaveCrc32(const std::uint8_t*,std::size_t) noexcept;
// Pure codec: no file I/O or publication. The base identity is supplied by the
// canonical-catalogue adapter, never inferred from a path/pack format version.
// v1 conserves the base population across team/free lists. Future created IDs
// require a new resolver/required section; wire IDs are never truncated.
std::vector<std::uint8_t> encodeRosterSave(const RosterSaveDocument&,
    const RosterSlots& base,const RosterBaseIdentity& identity);
RosterSaveDocument decodeRosterSave(const std::vector<std::uint8_t>&,
    const RosterSlots& base,const RosterBaseIdentity& identity);
}
