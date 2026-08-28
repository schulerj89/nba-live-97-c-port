# Resolve callback entry points missed by the initial automatic analysis.
# Run ONLY with analyzeHeadless -readOnly; these annotations are transient.
# Addresses come from recovered function pointers/dispatcher calls, not guesses.
for value in (0x80055ef0, 0x80056c50, 0x8005a3fc, 0x8005a6f0):
    address = toAddr(value)
    if getFunctionAt(address) is None:
        disassemble(address)
        function = createFunction(address, None)
        if function is None:
            raise RuntimeError("could not define callback at {}".format(address))
    print("[TradeCallback] {}".format(getFunctionAt(address)))
