# Minimal non-interactive function exporter for local headless Ghidra runs.

args = getScriptArgs()
if not args:
    raise RuntimeError("output CSV path argument is required")

output_path = args[0]
rows = []
manager = currentProgram.getFunctionManager()

for function in manager.getFunctions(True):
    if function.isThunk() or function.isExternal():
        continue
    body = function.getBody()
    size = body.getNumAddresses()
    if size < 4:
        continue
    rows.append((function.getName(), function.getEntryPoint().getOffset(),
                 body.getMaxAddress().getOffset(), size))

with open(output_path, "w") as output:
    output.write("Name,StartAddress,EndAddress,Size\n")
    for name, start, end, size in rows:
        output.write("{},0x{:08X},0x{:08X},0x{:X}\n".format(
            name, start, end, size))

print("[ExportFunctionsHeadless] Exported {} functions to {}".format(
    len(rows), output_path))
