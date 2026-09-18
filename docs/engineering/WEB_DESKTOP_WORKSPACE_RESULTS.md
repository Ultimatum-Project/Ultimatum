# Desktop web workspace verification

Verified September 15, 2026, in the actual browser/WASM runtime.

## Implementation

- Desktop (981px+) removes the duplicated location row and puts moves in the
  top bar. The game occupies the reclaimed space, with a bounded viewport.
- Creation, conversations, shops and menu prompts live in the right-hand
  Conversations panel, not a fixed overlay. Desktop prompts are nonmodal HTML
  regions; the engine still gates game mutations until the choice is finished.
- Intro rendering retains 320×200 rather than downsampling to the 176×176 world
  crop. Classic/VGA pixel art stays crisp rather than artificially smoothed.
- DOS hard wraps reflow as prose; deliberate blank paragraphs and lettered
  inventory lists survive. Latest command output remains in the panel footer.
- WASD supplements arrows in command, combat and direction modes. Shift plus
  a WASD letter sends the lowercase classic command; text entry, creation
  choices, composition and browser shortcuts are not remapped.
- NPC/shop context and accepted player replies become optional, bounded
  `conversations.json` save metadata (500 recent entries, 8192 chars/entry).
  Existing save slots/checkpoints/exports carry it; new games start empty and
  old saves without it remain compatible. Not a cloud archive or native iOS
  transcript implementation.
- Mobile retains its existing sheets and thumb controls. Resizing across the
  desktop breakpoint moves the same live prompt, not a duplicate controller.

## Actual runtime results

- Desktop-specific suite: 8 checks PASS. Full name entry, 24 story pages, seven
  virtue questions, unobscured artwork, all four WASD/arrow outcome comparisons
  with exactly one turn, Shift+S Search, NPC Name/Goodbye, restored controls,
  checkpoint history, Continue history restoration and export/import retention.
- Existing interaction suite: 23 scenarios PASS (shops/services, NPCs,
  Lord British, Hawkwind, recruitment, shrine, cancellation and stale/duplicate
  answers). The new desktop side-panel layout was used by the real controls.
- Phone-sized 393×700 HUD suite: 8 checks PASS; stable messages, Search/Wait,
  touch target fit, exact real turns, Menu geometry and Resume. This was a real
  browser iframe viewport, not a physical iPhone test.
- Public BYOD release: 37 import/creation/control/save/reload checks PASS;
  original game data and development saves/debug hooks remain excluded.
- Browser keyboard events: native W advanced the engine; native Shift+S
  produced Searching/Nothing Here rather than movement. Classic idle auto-pass
  remains engine-owned and may advance turns during extended manual inspection.
- Web unit tests: 27 PASS; site packaging tests: 3 PASS; JS syntax and whitespace
  checks PASS. Metadata round-trip tests now include conversation history.

## Packaging coordination

The full public build initially succeeded. A later rebuild encountered the
other task's newly-added upstream VGA fetch and unavailable sandbox networking.
The final UI was refreshed using `clients/site/scripts/refresh-web-shell.mjs`,
which first verifies content-addressed engine code/assets and every matching
source hash. It preserves the previously tested engine/native source snapshot
and refreshes only authored shell files plus their source manifest entries.
Concurrent native/VGA/music changes are intentionally not part of this commit.
No domain publishing, account changes or iOS device install was performed.
