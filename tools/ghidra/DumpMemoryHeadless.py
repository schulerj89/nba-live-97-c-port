# Dump a recovered address range from a headless Ghidra program.
# Output is derived evidence and must remain under .local.

args = getScriptArgs()
if len(args) != 3:
    raise RuntimeError("usage: <output-path> <start-address> <byte-count>")

output_path = args[0]
start = toAddr(int(args[1], 0))
size = int(args[2], 0)
data = bytearray(size)
currentProgram.getMemory().getBytes(start, data)

with open(output_path, "wb") as output:
    output.write(bytes(data))

print("[DumpMemoryHeadless] wrote {} bytes from {} to {}".format(
    size, start, output_path))
