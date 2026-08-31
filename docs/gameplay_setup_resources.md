# Owned gameplay setup resources

`GameplaySetup` owns one immutable motion resource generation, two original
formation tables and both period-duration lookup windows. It supplies data to
the recovered player/period owners. It does not initialize a whole match,
sample animation, create a court or implement gameplay.

Run `python tools/extract_gameplay_setup.py` to extract the original private
resources, or use the normal `scripts/extract_assetpacks.ps1` pipeline. The
extractor checks the exact original disc, GAMEONLY and ZMOCAP hashes. It writes
only below repository `.local`, refuses source/output and hardlink aliases,
checks every existing destination before publication, and leaves identical
files and their timestamps unchanged. Conflicting output requires a new private
folder; it never silently overwrites original or previously verified bytes.

`ZMOCAP.BIN` retains all200044 original bytes. `period_setup.bin` has an8-byte
NBA97PER magic, version1, payload length2112 and payloadCRC32, followed by:

| Payload | Bytes | Original data |
|---|---:|---|
|Two formation blocks |64 |B891C andB893C,32bytes apart; five signed triples each |
|Normal duration window |1024 |B895C plus every unsigned-byte option times4 |
|Overtime duration window |1024 |B8970 plus every unsigned-byte option times4 |

The original period owner does not limit the option byte to the ordinary five
menu choices. Both256-word windows retain its unchecked reads into adjacent
original data. This preserves raw-state behavior without inventing durations
or labeling those adjacent values as additional valid UI options. The windows
remain within the verified original GAMEONLY file and are never executed as code.

`loadGameplaySetup(folder)` validates the period pack and loads the immutable
motion owner. Publication happens only after all reads/validation complete;
failure leaves any previously held shared generation intact. Formation access
uses explicit table0/1. Duration lookup accepts the actual raw option byte.
`motionView(channel,slot)` combines normalized flags/count with unchanged byte+2
read from the retained raw header. A null directory entry stays absent, and an
invalid index throws; no synthetic header or default frame is provided.

Public synthetic checks cover endian/signed values, all512 duration accesses,
malformed packs, resource lifetime, absent motion entries and a composed
resolver-to-65B18 reset. That case preserves the original primary channel's
out-of-range frame copied from its secondary channel. The independent private
review compares actual C++getters against30 original formation values,
512 original duration words and168 motion views from executing GAME640D8.
All agree; see `.local/verification/native_completion/substitution/setup_review.json`.
The raw period pack hash is recorded in private `gameplay_setup.json`.

Host integration must retain this resource while any consumer uses its data,
copy the full duration windows into the period coordinator input, and provide
the selected formation and actual motion views to65B18. Neither the resource
getter checks nor source-instruction checks establish an original live-game
capture or a playable native scene.
