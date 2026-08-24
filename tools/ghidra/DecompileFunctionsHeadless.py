# Decompile selected function entry points from an existing local Ghidra project.
# Output is derived from the private game executable and must remain under .local.

from ghidra.app.decompiler import DecompInterface

args = getScriptArgs()
if len(args) < 2:
    raise RuntimeError("usage: <output-path> <function-address> [address ...]")

output_path = args[0]
decompiler = DecompInterface()
decompiler.openProgram(currentProgram)
lines = []

for value in args[1:]:
    address = toAddr(int(value, 0))
    function = getFunctionAt(address)
    if not function:
        function = getFunctionContaining(address)
    lines.append("FUNCTION {} {}\n".format(
        function.getName() if function else "<missing>", address))
    if not function:
        lines.append("<function not found>\n\n")
        continue
    result = decompiler.decompileFunction(function, 120, monitor)
    if result.decompileCompleted():
        lines.append(result.getDecompiledFunction().getC())
    else:
        lines.append("<decompile failed: {}>\n".format(result.getErrorMessage()))
    lines.append("\n")

with open(output_path, "w") as output:
    output.writelines(lines)

print("[DecompileFunctionsHeadless] wrote {}".format(output_path))
