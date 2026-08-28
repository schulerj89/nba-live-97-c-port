# Resolve callback entry points passed by FEONLY 80056F9C.
# Invoke ONLY with analyzeHeadless -readOnly; annotations are transient.
# Recomp fragments 80056D74/80056E48 omit the preceding input loads.
for value in (0x80056D6C, 0x80056E40):
    address = toAddr(value)
    if getFunctionAt(address) is None:
        disassemble(address)
        if createFunction(address, None) is None:
            raise RuntimeError("could not define Sign callback {}".format(address))
    print("[SignCallback] {}".format(getFunctionAt(address)))
