# Headless Ghidra pre-script for a raw-imported PS-X EXE payload.
# The complete EXE is mapped at load_address - 0x800 so its code payload
# begins at the address recorded in the PS-X EXE header.

args = getScriptArgs()
entry_text = args[0] if args else "0x801E3508"
entry = toAddr(int(entry_text, 0))

disassemble(entry)
if getFunctionAt(entry) is None:
    createFunction(entry, "psx_entry_{}".format(entry_text[2:].upper()))

print("[PreparePS1Raw] Entry point prepared at {}".format(entry))
