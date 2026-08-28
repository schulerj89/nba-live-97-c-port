# Resolve the callback pointer passed by FEONLY 8005721C.
# Invoke ONLY with analyzeHeadless -readOnly; annotations are transient.
# Recomp fragment8005708C omits the preceding two input-load instructions.
address = toAddr(0x80057084)
if getFunctionAt(address) is None:
    disassemble(address)
    if createFunction(address, None) is None:
        raise RuntimeError("could not define Release callback {}".format(address))
print("[ReleaseCallback] {}".format(getFunctionAt(address)))
