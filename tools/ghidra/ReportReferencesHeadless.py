# Report references and decompiler output for selected addresses.
# Output belongs under .local because it is derived from the game executable.

from ghidra.app.decompiler import DecompInterface

args = getScriptArgs()
if len(args) < 2:
    raise RuntimeError("usage: <output-path> <address> [address ...]")

output_path = args[0]
addresses = [toAddr(int(value, 0)) for value in args[1:]]
functions = {}
lines = []

for address in addresses:
    lines.append("TARGET {}\n".format(address))
    references = getReferencesTo(address)
    if not references:
        lines.append("  no references\n")
    for reference in references:
        caller = getFunctionContaining(reference.getFromAddress())
        caller_name = caller.getName() if caller else "<no function>"
        lines.append("  {} {} from {} ({})\n".format(
            reference.getReferenceType(), reference.getFromAddress(),
            caller_name, caller.getEntryPoint() if caller else "n/a"))
        if caller:
            functions[caller.getEntryPoint().getOffset()] = caller

decompiler = DecompInterface()
decompiler.openProgram(currentProgram)
for entry in sorted(functions):
    function = functions[entry]
    result = decompiler.decompileFunction(function, 60, monitor)
    lines.append("\nFUNCTION {} {}\n".format(function.getName(),
                                             function.getEntryPoint()))
    if result.decompileCompleted():
        lines.append(result.getDecompiledFunction().getC())
    else:
        lines.append("<decompile failed: {}>\n".format(result.getErrorMessage()))

with open(output_path, "w") as output:
    output.writelines(lines)

print("[ReportReferencesHeadless] wrote {}".format(output_path))
