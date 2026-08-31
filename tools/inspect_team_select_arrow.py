"""Validate one saved Team Select arrow-construction capture, without process access.

PC and S0 must be recorded from the debugger at the same stop as the RAM dump.
They are caller-supplied observations, never inferred from saved RAM. Four
separate stops/receipts are needed to establish all four initial arrow colors.
Original bytes and the resulting report must remain private.
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE_SHA256 = "14904a5644a517f3799a8ac0b5a5b010a2f57752cf1c9ff64ac97e9d3d32a94c"
BASE = 0x80015000
ANCHORS = ((0x80029B98,0x80029DD0), (0x8002C6B0,0x8002CEBC),
           (0x8003D434,0x8003D534), (0x8003D930,0x8003E620),
           (0x8003F7C8,0x80040A1C), (0x8004FA3C,0x8004FC80),
           (0x8004FCD8,0x8004FDE8))


def require(ok, message):
    if not ok:
        raise ValueError(message)


def inspect(ram, source, pc, record):
    require(len(ram) == 0x200000, "expected exactly 2 MiB of PS1 main RAM")
    require(hashlib.sha256(source).hexdigest() == SOURCE_SHA256, "unrecognized FEONLY source")

    def raw(address, size):
        require(type(address) is int and 0x80000000 <= address <= 0x80200000-size,
                "expected a bounded KSEG0 main-RAM pointer")
        start = address-0x80000000
        return ram[start:start+size]

    def u(address, size=4, signed=False):
        return int.from_bytes(raw(address, size), "little", signed=signed)

    for start, end in ANCHORS:
        require(raw(start,end-start) == source[start-BASE:end-BASE],
                f"FEONLY code mismatch at {start:08X}; do not interpret reused overlay addresses")
    require(pc == 0x8003D51C, "requires debugger PC8003D51C after the new arrow-pointer store")
    ctx, ctrl, manager = (u(a) for a in (0x800170C0,0x80022088,0x8009352C))
    require(ctx%4 == 0 and manager%4 == 0, "unaligned frontend context/text manager")
    raw(ctx,0x87C)
    require(u(ctx+0x720,2) == 3, "capture is not frontend state3")
    require(ctrl == 0x800C12EC and u(ctrl,1) == 14 and u(ctrl+0x10,1) == 2,
            "capture is not the constructed Team Select controller")
    require(record in [ctrl+0x48+16*i for i in range(4)], "S0 is not one of the four arrow records")
    index = (record-ctrl-0x48)//16
    page = u(ctrl+0xF,1)
    require(page in (0,1), "invalid Team Select page")
    require(u(record+4) == 0 and u(record+10,2) == 1,
            "requires unconditional, newly existing Team Select arrow")
    xy = [(320,96),(460,96),(-458,96),(-318,96)][index]
    require((u(record+12,2,True),u(record+14,2,True)) == xy, "unexpected arrow descriptor anchor")
    raw(manager,0x58)
    pool, heads, capacity = u(manager+0x10),u(manager+0x14),u(manager+0x22,2)
    require(pool%4 == 0 and heads%2 == 0, "unaligned text pool/group heads")
    require(capacity == 200, "unexpected frontend text-pool capacity")
    raw(pool,capacity*64)
    hint, buffer_page = u(manager+0x40,2),u(manager+0x53,1)
    require(hint < capacity and buffer_page in (0,1), "invalid text-manager hint/page")
    node = u(record)
    require(pool <= node < pool+capacity*64 and (node-pool)%64 == 0, "arrow does not identify a complete pool slot")
    slot = (node-pool)//64
    require(slot == hint, "new arrow is not the allocator's last chosen slot")
    group = u(node+0x14,2)
    require(group == 120+page and u(heads+2*group,2) == slot, "new arrow is not the current-page group head")
    require(u(node+0x12,2) == 32767 and u(node+0x3B,1) == 0,
            "arrow lifetime/flags do not match fresh construction")
    require((u(node+0xE,2,True),u(node+0x10,2,True)) == xy and
            u(node+0x1E,2) == u(node+0x20,2) == 0, "new arrow already moved")
    require(u(node+0xC,2) == 1, "expected one glyph with two primitive pages")
    primitive = u(node+8)
    require(primitive%4 == 0, "unaligned arrow primitive")
    primitive_bytes = raw(primitive,80)
    require(all(primitive_bytes[page*40+4:page*40+7] == b"\x80\x80\x80" for page in (0,1)),
            "new glyph primitive color is not neutral on both pages")
    return {
        "scope":"one source-consistent arrow construction; not a complete runtime, timing or universal seed proof",
        "register_provenance":"PC and S0 supplied by caller; saved RAM cannot prove their observation",
        "pc_from_debugger":pc, "s0_from_debugger":record,
        "ram_sha256":hashlib.sha256(ram).hexdigest(), "source_sha256":SOURCE_SHA256,
        "matched_code_bytes":sum(end-start for start,end in ANCHORS),
        "index":index, "page":page, "context":ctx, "controller":ctrl,
        "manager":manager, "pool":pool, "capacity":capacity, "slot":slot,
        "last_chosen_hint":hint, "buffer_page":buffer_page, "group":group,
        "arrow_record_hex":raw(record,16).hex(), "node":node,
        "node_hex":raw(node,64).hex(), "start_rgb":list(raw(node+0x30,3)),
        "primitive":primitive, "primitive_pages_hex":primitive_bytes.hex(),
        "home":u(0x80021D74), "away":u(0x80021D78),
        "clock120":u(0x800D9AB8), "vblank":u(0x800D9AD0)
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ram",type=Path)
    parser.add_argument("--source",type=Path,default=ROOT/".local/extracted/FEONLY.BIN")
    parser.add_argument("--pc",type=lambda value:int(value,0),required=True)
    parser.add_argument("--s0",type=lambda value:int(value,0),required=True)
    parser.add_argument("--out",type=Path,required=True)
    args = parser.parse_args()
    output = args.out.resolve()
    require(output.is_relative_to((ROOT/".local").resolve()), "raw capture receipt must remain under .local")
    require(not output.exists(), "receipt already exists; use a fresh output path")
    report = inspect(args.ram.read_bytes(),args.source.read_bytes(),args.pc,args.s0)
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open("x",encoding="utf-8") as stream:
        json.dump(report,stream,indent=2)
        stream.write("\n")
    print(f"ARROW CAPTURE: source-consistent index{report['index']} slot{report['slot']}; caller-supplied debugger stop; {output}")


if __name__ == "__main__":
    main()
