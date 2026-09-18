# Compact web HUD runtime verification

Verified 2026-09-17 UTC against the real fixture-enabled Emscripten engine and
shipped HTML controls in the in-app browser. No iOS engine or app changes.

## Layout and behavior

Normal phone feedback no longer reserves a separate row. Up to three recent
lines render non-interactively inside the world stage; complete history remains
in Log. Semantic conversations and Menu retain temporary modal sheets.

Phone web now follows the native composition: compact status and live minimap,
a width-dominant game view, lower-left D-pad, and lower-right 2×4 action deck.
The deck changes by mode. Exploration exposes Party, Spells, Journal, Menu,
context action, Map, Talk and Wait; combat exposes target cycle, Clear, Attack,
and the native-style contextual Repeat attack button in the D-pad center;
Attack; dungeons expose Torch, the ladder/context action, view toggle and one
Search button. Portal branding is removed from the active phone play surface;
Game data remains available from the journey sheet footer.

## Evidence

- Portrait, actual 393 × 700 viewport: all HUD assertions passed against the
  real fixture engine. The world stage measures 375 × 420px and the square game
  view is 375 × 375px; every control target is at least 44px.
- Normal feedback consumes 0 layout pixels. The in-world overlay remains capped
  at three lines and long feedback cannot resize the world. Four live Wait
  actions produced exactly three rendered `Pass` lines.
- The parity suite passes the live world map/pin, dungeon view/floor map and
  combat target selection/clear flows. It also asserts that the visible dungeon
  deck contains exactly one Search action.
- Landscape, actual 844 × 390 frame viewport: all six HUD assertions passed;
  controls remain visible and touch targets are at least 44px.
- Desktop, 1280 × 720: all six HUD assertions passed. Desktop remains a
  scrollable layout, unlike the viewport-bound mobile shell. Commands are
  checked against their own action-area bounds to prevent horizontal overflow
  into the adjacent party panel.
- Wait advances one engine turn without changing party coordinates. Menu and
  Resume consume no turn, preserve world geometry, and lock/restore background
  controls.
- The pagehide/pruned-tab suite passed with the exact saved move count restored
  after a full document reload.
- All 23 special-conversation regressions passed in the portrait frame.
- All 53 Menu regressions passed in the portrait frame, followed by a real
  frame reload verifying persistent preferences and reset session switches.
- All 32 unit/contract tests passed; JavaScript syntax and diff checks passed.
- Public music-free package revision `20260917-mobile6` built successfully. It
  was not deployed as part of this UI implementation.

The responsive fixture embeds the app in a same-origin, precisely sized
iframe, so media queries use the observed frame dimensions even when a browser
viewport override is unavailable. Fixture pages/drivers are test-only and
never run on the user's LAN game origin. These browser checks are not physical
iPhone Safari verification.

## Reproduction

Build the isolated runtime engine and start the fixture server as described in
`clients/web/README.md`, then open:

- `http://127.0.0.1:4174/responsive-runtime.html`
- `http://127.0.0.1:4174/responsive-runtime.html?layout=landscape`
- `http://localhost:4174/?hud_suite=1`
- `http://127.0.0.1:4174/responsive-runtime.html?suite=conversation`
- `http://localhost:4174/responsive-runtime.html?suite=menu`

Each driver publishes its assertions in `#runtimeReport`. Gameplay assertions
use actual snapshots after shipped-control clicks. Only the short/long-text
and old-strip comparison deliberately alter test-page presentation; they do
not mutate game state or replace engine snapshots.

## Edge-newline cleanup (shell revision 20260915t)

The classic door-opening command emits a leading newline for screen/cursor
positioning. HTML message rendering now trims edge whitespace and drops
whitespace-only entries, in both compact feedback and Log. Internal newlines
and paragraph breaks are preserved; raw engine history remains untouched.
The fixed-height message strip and semantic conversation layout are unchanged.

All eight portrait real-engine tap/interaction cases passed with the added
door-message assertions: the actual snapshot contained `"\nOpened!"`, while
both the strip and Log contained exactly `"Opened!"`. All eight compact-HUD
assertions passed at 393 × 700. All 13 unit/contract tests passed, including
edge padding, whitespace-only entries, preserved paragraphs, NPC prompt
filtering, and HTML escaping. This remains browser-frame QA, not physical
iPhone Safari verification.
