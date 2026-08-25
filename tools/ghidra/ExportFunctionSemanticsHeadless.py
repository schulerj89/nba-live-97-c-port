# Export non-copyrightable structural metadata for selected original functions.
# Original instructions, bytes, and decompiler text are deliberately omitted.

import json

from ghidra.program.model.block import BasicBlockModel
from ghidra.program.model.scalar import Scalar


args = getScriptArgs()
if len(args) < 2:
    raise RuntimeError("usage: <output-json> <function-address> [address ...]")

output_path = args[0]
requested = [int(value, 0) for value in args[1:]]
listing = currentProgram.getListing()
block_model = BasicBlockModel(currentProgram)


def hex_address(address):
    return "0x{:08X}".format(address.getOffset() & 0xffffffff)


def instruction_count(address_set):
    count = 0
    instructions = listing.getInstructions(address_set, True)
    while instructions.hasNext():
        instructions.next()
        count += 1
    return count


functions = []
for requested_address in requested:
    address = toAddr(requested_address)
    function = getFunctionAt(address)
    if function is None:
        function = getFunctionContaining(address)
    if function is None:
        raise RuntimeError("function not found at 0x{:08X}".format(requested_address))

    body = function.getBody()
    blocks = []
    edges = []
    block_iterator = block_model.getCodeBlocksContaining(body, monitor)
    while block_iterator.hasNext():
        block = block_iterator.next()
        block_start = block.getFirstStartAddress()
        block_end = block.getMaxAddress()
        block_record = {
            "start": hex_address(block_start),
            "end": hex_address(block_end),
            "instruction_count": instruction_count(block),
        }
        blocks.append(block_record)
        destinations = block.getDestinations(monitor)
        while destinations.hasNext():
            destination = destinations.next()
            destination_address = destination.getDestinationAddress()
            # Calls are tracked separately. A function's structural CFG only
            # contains destinations inside its own body.
            if destination_address is None or not body.contains(destination_address):
                continue
            edges.append({
                "from": hex_address(block_start),
                "to": hex_address(destination_address),
                "type": str(destination.getFlowType()),
            })

    calls = set()
    direct_call_site_count = 0
    data_references = set()
    scalar_constants = set()
    delay_slot_instructions = 0
    instructions = listing.getInstructions(body, True)
    while instructions.hasNext():
        instruction = instructions.next()
        if instruction.isInDelaySlot():
            delay_slot_instructions += 1
        for reference in instruction.getReferencesFrom():
            reference_type = reference.getReferenceType()
            target = reference.getToAddress()
            if target is None:
                continue
            if reference_type.isCall():
                calls.add(hex_address(target))
                direct_call_site_count += 1
            elif reference_type.isData() and not body.contains(target):
                data_references.add(hex_address(target))
        for operand_index in range(instruction.getNumOperands()):
            for operand_object in instruction.getOpObjects(operand_index):
                if isinstance(operand_object, Scalar):
                    value = operand_object.getSignedValue()
                    if -0x100000 <= value <= 0x100000:
                        scalar_constants.add(value)

    blocks.sort(key=lambda item: int(item["start"], 0))
    edges.sort(key=lambda item: (int(item["from"], 0), int(item["to"], 0), item["type"]))
    functions.append({
        "address": hex_address(function.getEntryPoint()),
        "ghidra_name": function.getName(),
        "size_bytes": int(body.getNumAddresses()),
        "instruction_count": instruction_count(body),
        "delay_slot_instruction_count": delay_slot_instructions,
        "basic_block_count": len(blocks),
        "control_flow_edge_count": len(edges),
        "blocks": blocks,
        "edges": edges,
        "direct_calls": sorted(calls, key=lambda value: int(value, 0)),
        "direct_call_site_count": direct_call_site_count,
        "external_data_references": sorted(data_references, key=lambda value: int(value, 0)),
        "scalar_constants": sorted(scalar_constants),
    })

functions.sort(key=lambda item: int(item["address"], 0))
report = {
    "schema_version": 1,
    "binary": "feonly",
    "processor": str(currentProgram.getLanguageID()),
    "functions": functions,
}
# Jython's Python 2 json encoder emits a space before some newlines when
# pretty-printing. Strip line endings so generated evidence remains clean and
# deterministic across the supported Ghidra runtime.
serialized = json.dumps(report, indent=2, sort_keys=True)
serialized = "\n".join(line.rstrip() for line in serialized.splitlines())
with open(output_path, "w") as output:
    output.write(serialized)
    output.write("\n")

print("[ExportFunctionSemanticsHeadless] Exported {} functions to {}".format(
    len(functions), output_path))
