# Special-conversation runtime verification

## Method

Run the real Emscripten engine and responsive production shell on an isolated
loopback origin. Test-only C++ fixtures place a seeded party beside authored
NPCs and enter the actual `talkAt`/shrine flow through the SDL event loop.
The page driver clicks the rendered semantic options or submits the actual
conversation form. It does not mock engine snapshots or vendor scripts.

The fixtures are compiled only with `ULTIMATUM_WEB_RUNTIME_TESTS=ON`, into
`.cache/runtime/engine`; normal `dist/engine` builds do not export fixture hooks.
The server binds `127.0.0.1:4174`, separate from the normal LAN server and its
browser storage. Never run these fixtures against a user's real adventure.

From `clients/web`, after preparing the normal engine dependencies/local data:

```sh
npm run test:runtime:build
npm run test:runtime:serve
```

Open `http://127.0.0.1:4174/?suite=1` in the test browser. The page's
`#runtimeReport` records each answer, prompt generation, assertions, and before/
after resource totals. A completed run must say `SUITE PASS` with 23 results.
Use `?fixture=1&manual=1` to inspect a weapon shop without automatic answers;
`?fixture=4&mode=ale` tests tavern rumours, and `?fixture=14&cancel=1` cancels
meditation. The fixture server serves development-only bundled game data;
it is not a deployable/public BYOD build.

## Coverage

Verified in the Codex in-app WebKit browser on 2026-09-14/15: the 23-case
suite passed at 390×844 portrait-phone and 1280×900 desktop viewports. This includes leader
healing to full HP and companion healing from 20 to 300 HP while the leader
remained at 300 HP. Desktop animation intervals also reject contextual actions,
world taps, saving, and equipment changes at the engine boundary. The user's
normal LAN server and adventure were untouched.

Manual browser interaction also completed a portrait Staff purchase (20 gold,
one additional weapon) and returned to enabled controls. At 844×390 landscape,
the large sell list scrolls to its final options and Cancel restores controls
without payment. Sheet backgrounds/headings were visually inspected after the
readability adjustment. The test report overlays the page's upper-left corner;
it is never injected into the normal LAN client.

| Flow | Observable proof |
|---|---|
| Weapons/armour buy and sell | Inventory changes in the correct direction; gold spent/received |
| Food | Food increases and gold decreases |
| Tavern food and ale/rumour | Payment; ale branch submits a free-text topic |
| Reagents, inn, guild, stable | Named goods/services accepted; gold decreases |
| Healer | Paid healing; selected companion healed without healing the leader |
| Beggar | Exactly ten gold donated; cancelled donation costs nothing |
| Recruitment | Party size increases |
| Lord British | Introduction continuation, health topic, confirmation, full party healing, exit |
| Hawkwind | Virtue topic, response continuation, exit |
| Ordinary NPC question | Authored question keyword, yes/no answer, exit |
| Shrine | Virtue, cycle count, mantra, vision continuation, return outside |
| Cancellation | Food/reagent quantities, donation amount, shrine mantra |

Every answered prompt also rejects duplicate and previous-generation answers.
Amount prompts reject nonnumeric, negative, and over-length input. Every flow
must restore command mode, enabled main controls, and a closed interaction
sheet. During yielding service animations, commands remain disabled and the
interaction sheet remains open.

## Defects discovered through runtime testing

1. Carriage-return serialization stripped the value of **Continue**, stalling
   Lord British's introduction. The bridge now JSON-escapes carriage returns.
2. Yielding healing effects temporarily exposed the outer GameController as
   command mode before the interaction completed. An engine-owned interaction
   scope now keeps that interval busy, blocks overlapping gameplay mutations,
   freezes idle turns, and preserves the temporary mobile sheet.
3. Portrait visual QA found legacy leading blank lines and game pixels showing
   through the sheet. Replies now trim surrounding whitespace; sheets are
   opaque and their headings cannot shrink out of view.

## Limits

These fixtures verify modal transitions, input validation, and actual engine
mutations—not the exploration route to each NPC or the outer Talk command's
turn accounting. Shrine timing is accelerated, without replacing its sequence.
They do not cover every item, rumour, virtue, rejected/insufficient-funds branch,
blood donation, curing, resurrection, elevation, or ending. Basic browser console
diagnostics still include existing SDL target/timing warnings; successful flow
assertions are not a claim of a warning-free integration.

Browser viewport testing is not physical iPhone Safari testing. Android Chromium,
physical-device keyboard/audio/interruption behavior, and complete adventure
parity remain separate verification work.
