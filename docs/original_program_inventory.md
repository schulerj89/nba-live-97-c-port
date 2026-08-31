# Original program inventory

The progress inventory now includes five known NTSC-U disc programs. Entries
are Ghidra-discovered function metadata, not translated code, a claim of all
possible entry points, or a game-completion score. Function identity includes
the binary: FEONLY and GAMEONLY occupy the same runtime range at different
times, and the two loader files share a code layout but have different hashes.

| Program | Runtime mapping | Entry | Functions | Code bytes |
|---|---|---|---:|---:|
|Boot PS-X EXE |Payload801E0000; complete-file import801DF800 |801E3508 |272 |49,280 |
|FEONLY |80015000 |8007B79C |1,497 |416,088 |
|FELOAD |801E0000 |801E1410 |106 |16,316 |
|GAMELOAD |801E0000 |801E1410 |106 |16,316 |
|GAMEONLY |80015000 |80094828 |1,720 |546,056 |

Total:3701 discovered functions and1,044,056 analyzed code bytes. Duplicated
SDK routines are retained per program; this is not a count of unique source
algorithms. Existing evidence records and milestone statuses were not promoted
when the denominator expanded. Those catalogues remain incomplete.

The disc's directory records place FELOAD atLBA37, FEONLY56, GAMELOAD525 and
GAMEONLY544. Their first words agree with the entry points above. Loader layout
is established by raw801E0000 imports and their absolute code/data references.
GAMELOAD801E136C explicitly loads GAMEONLY at80015000 and calls its entry word
after restoring the saved resident range. The earlier generated overlay file's
8001E800 FEONLY and800F9800 GAMEONLY mappings must not be used as runtime bases.

The canonical sizes/hashes and public inventory paths are in
`config/decomp/project.json`. Only names, addresses, extents and sizes are
published in `config/decomp/functions/*.csv`; original bytes and disassembly
remain private. Fresh exports use Ghidra11.3 with
`tools/ghidra/ExportFunctionsHeadless.py`. FELOAD was imported separately using
MIPS little-endian32,801E0000 base and801E1410 entry; GAMELOAD/GAMEONLY exports
reuse the established audit projects read-only.

Private `.local/verification/native_completion/program_inventory/receipt.json`
records exact disc identity, program extents, CSV hashes and counts. Its source
validation checks every exported function fits the declared original file,
has a unique aligned entry and a valid extent, and includes the entry point.
Original inputs that already existed were compared without rewriting them.

Regenerate reports with `python tools/report_progress.py`; check with
`python tools/report_progress.py --check` and `python tools/test_report_progress.py`.
The report validates any original input present locally, but stays reproducible
on asset-free checkouts. Analysis does not establish whole-program reachability,
computed targets, all modes/resources, runtime fidelity or playable basketball.
