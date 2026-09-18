# Web parity and lifecycle runtime results

**Date:** 2026-09-17

The isolated Emscripten runtime was exercised in the shipped responsive shell with development data and a dedicated browser origin. No user adventure was modified.

Verified in the real engine:

- world snapshot declared exploration maps and pins, rendered a live 33×33 minimap, opened the 256×256 adventure-owned map, and added/removed a pin through engine state;
- dungeon snapshot exposed the explored 8×8 current floor, rendered its contextual action bar, toggled the real engine between first-person and overhead views, and opened the floor map;
- combat snapshot identified the active party member, exposed a legal target, rendered one target overlay, selected it through semantic action dispatch, enabled Attack, and cleared the prepared target;
- a `pagehide` lifecycle event secured move 1601, a full document reload simulated a pruned browser tab, and the authoritative active slot resumed automatically at move 1601 without reopening the title dialog.

The runtime pass found and fixed two boundary defects: the SDL web event allowlist initially discarded the new semantic gameplay-action range, and the web snapshot initially read the non-combat party active index instead of the combat controller's current actor. Both now have source-level regression assertions.

The runtime pass used the then-default public package with Debug Tools compiled out. The current early-access policy enables the same compile-gated tools in both production and QA packages; their cloud-project metadata, Worker configuration, and origins remain separate.

Remaining device coverage: repeat the background/termination matrix on physical iOS Safari and Android Chromium, where process eviction timing cannot be forced deterministically from a desktop fixture.
