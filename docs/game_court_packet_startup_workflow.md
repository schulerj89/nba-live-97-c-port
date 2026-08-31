# Court packet startup interval

`game_court_packet_startup.c` owns the exact 163-word GAME interval
`484B8..48744` inside `479B8`. It begins with the required real `994F4(0)`
synchronization and ends immediately before the independently frozen `48744`
texture-selection bridge. It does not claim the preceding roster/interactive
owners naturally reached it, or that a court frame, possession, or match ran.

After sync, the source scans the ten bits of scratchpad word `1F80000C`. A set
bit selects the corresponding `F0ED8 + BCC * player` context and publishes that
original address to `F0ED4`. Each selected context walks exactly twenty packet
descriptors at `context + B0 + 94 * group`. It reads the descriptor pointer,
signed count, source packet base at `+8`, and destination packet base at `+C`.
Nonpositive signed counts skip. Positive counts patch packets at 32-byte
stride, calling the actual `9BF98(2,0,200,100)` service for every packet.

The page halfword is written to both source and destination offset `16`.
Source bytes `C,D,14,15,1C,1D` become `0,U,10,U,0,V`, then are copied to the
destination packet in source order. The U/V bytes begin at `5F/6F` and advance
by `10` for every player, including unselected players. The source descriptor
pointer and its count are reread after every page call; source and destination
bases remain the earlier captured values. A callback can therefore change the
next loop decision without retroactively changing the current packet bases.

The selected context's `BC4` descriptor is a distinct second packet bank. Its
count and bases are at `+0`, `+28`, and `+2C`. It uses the same page and copy
sequence but deliberately starts V at `6E`, one less than the body groups.
After every page call this route rereads `F0ED4`, then the current context's
`BC4` pointer and count. This original live reload is retained rather than
cached. A selected player finally stores `2` to `8010B270` and `20` to both
`FDA04` and `FDA08`; repeated selected players repeat those stores.

## Binding and refusal contract

The entry uses retained raw source-address regions and per-byte knownness. It
does not construct contexts, descriptors, packet arrays, mask bits, page
values, or graphics mode. `55F0C` is represented by its exact raw scratchpad
word read. `994F4` and `9BF98` remain explicit synchronous I/O boundaries;
production must run their real owners and cannot acknowledge them with a
success stub. Page results must be known. Callbacks may mutate retained memory,
and all source reloads after a completed callback observe those mutations.

Alignment traps, unknown consumed bytes, missing mappings, malformed metadata,
access bounds, event bounds, and refused services stop natively. Ordered stores
and completed service effects before a stop remain visible. The owner is not
transactional or resumable; an atomic host publication must stage retained
memory and the sync/page service state together. Private source stack/code may
not alias visible inputs.

## Verification boundary

Private evidence lives under
`.local/verification/native_completion/court_packet_startup/`. The source
audit hashes and lists all 163 words from the authoritative GAME overlay. A
bounded original-CPU comparison executes the raw interval with branch/load
delays, hooks only the declared scratch-read/sync/page boundaries, and compares
ordered stores, full service arguments/results, retained RAM/knownness, live
reload mutations, and refusal prefixes against the C owner. Strict MSVC Debug
and Release plus GCC C99/C++17 UBSan builds each pass 45 public checks. Each
Debug/Release original comparison covers 435 cases, 65,931 ordered CPU stores,
4,715 service events, 778,574 original instructions, 341 event-capacity
prefixes, four explicit service-refusal cases, and every one of the 163 owned
PCs. None of this is natural startup or gameplay evidence.
