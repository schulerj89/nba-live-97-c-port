# Run with analyzeHeadless -readOnly. These annotations are never saved.
# The Reset callback begins with two input loads that the recomp places
# after the preceding function as data; its named fragment starts at57968.
address = toAddr(0x80057960)
if getFunctionAt(address) is None:
    disassemble(address)
    if createFunction(address, None) is None:
        raise RuntimeError("could not define Reset callback {}".format(address))
print("[ResetCallback] {}".format(getFunctionAt(address)))
