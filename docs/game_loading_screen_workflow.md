# GAMEONLY loading-screen compositor recovery

`src/recovered/game_loading_screen.c` owns the complete 50-instruction
GAMEONLY routine `0x80029E58..0x80029F1F`. Main reaches it at call PC
`0x80029AE4`, immediately after match-session owner `0x8002D8D4` returns and
before main re-enters the FELOAD transfer path. A fresh read-only Ghidra
extraction has instruction SHA-256
`a7cd09cf9222d55787b6188292a434ef2d3645f61fc8cbe214251ac39827bf7e`.

The routine is the post-match loading-screen compositor. Its exact ordinary
path is:

```text
0x80029BFC("zloadscr.psh", 0) -> archive
if archive == 0: return normally
0x800A5478(archive, "LdS1") -> image
DrawSync(0)
0x800946B8(image,   0,   0, 0, 0)
DrawSync(0)
0x800946B8(image,   0, 256, 0, 0)
DrawSync(0)
0x800946B8(image, 512,   0, 0, 0)
DrawSync(0)
0x80090698(archive)
restore the live o32 frame and return
```

The strings are original GAMEONLY data: `zloadscr.psh` begins at
`0x800247F8`, and lookup key `LdS1` begins at `0x80024808`. Archive loading,
entry lookup, synchronization, image transfer and heap release remain typed,
mandatory synchronous boundaries. The native diagnostic gives them concrete
retained fixture effects; the recovered owner itself never invents a file,
pixel, successful GPU operation or release.

The archive-load boundary is no longer a bare return fixture in the composed
diagnostic: it runs the complete recovered `0x80029BFC` retry wrapper, which
calls typed attempt boundary `0x800941C8`. The diagnostic makes that boundary
return null once and then `0x80130000`, proving the source retry. Production
file/device/allocation work at `0x800941C8` remains external.

Compatibility keeps the source's asymmetric null handling. A zero archive
handle silently skips lookup, every DrawSync, all uploads and release. There is
no corresponding check after the image lookup: even a zero image value reaches
all three `0x800946B8` calls. The four synchronization points, fifth upload
argument written as zero in each JAL delay slot, final release `v0`, and live
stack reload of `ra`, `s1`, then `s0` are also retained. An unknown lookup
value crosses the first DrawSync before the native model refuses its first
pointer use, preserving that observable prefix.

The standalone unit covers the ten-call ordinary path, exact coordinates and
register context, the silent missing-archive path, unchecked null image,
unknown loader/lookup values, callback refusal and malformed output at every
prefix, mutable/unknown epilogue storage, every operation budget, and memory
validation. `game_main` composition proves the natural call and return point.

`scripts/verify_game_entry_visual.ps1` remains self-driving: input comes from
the test's recovered Game Setup, Team Select and User Setup handlers, never
computer-control clicks. For this owner it creates a generated retained
512x240 16-bit `LdS1` image and runs the already-recovered `0x800946B8` image
owner three times. It captures visible-page before/after frames plus full
1024x512 VRAM before the routine, after the top-left upload, after the
bottom-left upload, and after the top-right upload. The verifier proves each
successive frame differs only inside `(0,0,512,240)`, `(0,256,512,240)`, then
`(512,0,512,240)`, respectively.

These PPMs show the compositor's placement behavior with generated diagnostic
pixels. They are not retail loading-screen art, a court frame, a possession,
or evidence that the frontend now launches gameplay.
