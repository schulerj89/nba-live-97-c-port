# no$psx reference controls

Verified in **Options > Controls Setup > Keyboard > Player 1** on
2026-08-28. These are this reference installation's bindings, not assumed
no$psx defaults and not the native port's bindings. Recheck the setup UI if
the emulator configuration changes; do not infer mappings from key names.

| Keyboard key | PlayStation button |
| --- | --- |
| Arrow keys | D-pad |
| F | Triangle |
| C | Cross (X on the controller) |
| D | Square |
| V | Circle |
| **Right Shift** | **Select** |
| Enter | Start |
| A | L1 |
| Z | L2 |
| L | L3 |
| S | R1 |
| X | R2 |
| R | R3 |

L3/R3 are listed by the setup UI; the configured controller is digital, so
this does not establish in-game support for those buttons.

## Reference-testing notes

- Focus the **No$psx Emulator** game window before pressing game keys.
- In Trade Players, F opens Help, C picks/trades, and D opens View Player.
- Right Shift opened the original dirty-Trade discard confirmation after
  the Mason/McIlvaine trade on 2026-08-28. Leave it open when asked to capture
  the dialog; opening it does not itself confirm discarding.
- **Keyboard X is R2, not Select or the controller's Cross button.** Earlier
  instructions identifying X as original Cancel were incorrect. Earlier
  observations following an unspecified "Done" do not prove that key mapping.
- S is R1, not Circle. Earlier Compare observations after S must be treated
  as observed behavior, not proof that S maps to Circle; use the game's Help
  and recovered input masks to establish each context's action.
- Recomp callbacks `80056B44` and `80056C50` explicitly allow only `0x10`
  (Square/View) and `0x40` (Circle/Compare) through to `80054B94`. Shoulder
  inputs fall through and clear the selector sound latch. For the next
  original Trade Compare test use **V (Circle)**, not S. The earlier
  S-associated observation is not sufficient evidence to add an R1 alias.
- Do not copy these bindings into the port automatically. The port currently
  uses X/Escape for cancel and S for Compare; see [native controls](../README.md#keyboard-controls).

Original screenshots and recordings stay private under `.local/verification/`.
This document contains no game assets or emulator configuration files.

## Sign reference session: input diagnosis (2026-08-28)

The running emulator continued animating, and the debugger reported Running.
Two supported computer-use F key injections did not open game Help, including
one after foreground activation. The same interface successfully opened the
debugger's Options menu with Alt+O. Controls Setup still showed the bindings
above. The user subsequently opened Sign Help with physical F input.

This rules out a paused game or a changed F mapping in this session. It does
**not** prove a particular root cause: short key duration, scan-code handling,
or the game's polling path remain hypotheses. The available key API has no
separate hold/down/up controls to isolate these possibilities. Sending Escape
to the debugger while its child settings dialog was open also failed; that
parent/child targeting observation is separate from the in-game input issue.
No emulator bindings or security settings were changed.

Use manual game inputs and computer-use screenshots for reference validation
until reliable game-key injection is demonstrated. Native capture harnesses
exercise host handlers directly; they do not prove Windows input delivery.
Do not label this an emulator bug or claim automated input has been repaired.
The same supported F injection did open Help in the native Sign screen in a
live keyboard smoke test; this was observed separately from the host harness.
