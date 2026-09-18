# Ultimatum web client study

## Desktop workspace

At widths of 981px and above, location and moves live in the top bar. The
right-hand Conversation tab hosts creation, dialogue and menu prompts without
covering the world; a compact latest-message strip remains in the panel footer.
Intro graphics retain the full 320×200 source resolution rather than being
downsampled into the 176×176 world crop. Gameplay remains crisp, pixelated art.

Use arrow keys or WASD to move (also while choosing a direction). Unmodified
WASD takes precedence over classic letter commands; Shift plus the letter keeps
those commands available, and their visible buttons/menu entries still work.
Typing in inputs, browser shortcuts and character-creation choices are not
remapped. Mobile keeps its existing prompt sheets and thumb-control layout.

Conversation shows the current exchange and reply controls. Log contains recent
gameplay actions/feedback, without duplicating whole NPC conversations. Journal
is the adventure's durable knowledge: encountered passages grouped by source,
bookmarked Current Clues, attached notes, and standalone My Notes. It opens as
a paused reading/editing dialog (full-screen on phones), including during an
NPC conversation; closing restores the exact prompt and unsent reply draft.

New revealed dialogue and shrine visions use the native `topics.txt` journal.
Bookmarks and personal notes use iOS's `journal-notebook.dat` format. Done
durably saves note edits; Cancel discards the draft. Metadata-only transactions
preserve gameplay checkpoint bytes and their previous recovery generation.
Notes never expose unseen game topics. Search covers revealed passages and notes.

Earlier NPC/shop responses and player replies are recorded in `conversations.json`,
an optional adventure-owned file. Recent history is capped at 500 entries and
is included in checkpoints and `.u4save` exports, separated by save slot through
the existing adventure lifecycle. Old saves without the file remain compatible.
Save the adventure before leaving to retain the latest dialogue. This is local
save metadata, not an account-backed or cloud conversation archive. Earlier
transcripts remain read-only in Journal, not mixed into the active reply view.

Isolated runtime QA: `python3 tests/serve-runtime.py --port 4184`, then open
`http://127.0.0.1:4184/?desktop_suite=1` at desktop width on a fresh test origin.
The suite uses the existing runtime fixture engine; test assets/hooks are not
copied into public releases.

Journal runtime QA: use a fresh disposable loopback origin with
`?journal_suite=1`. The suite exercises real NPC dialogue, Journal DOM actions,
paused timers, native notebook validation, IndexedDB failure rollback,
checkpoint preservation, and Continue after reload. Results and boundaries are
documented in `docs/engineering/WEB_JOURNAL_RESULTS.md`.

For a UI-only refresh during concurrent native development, run
`node clients/site/scripts/refresh-web-shell.mjs` from the repository root.
It verifies the existing public engine/source hashes, updates the shell and its
source manifest, and leaves compiled assets and matching native sources intact.
Use the normal full site build when changing the engine.

The public homepage is a separate client at `clients/site`. Its build packages
this gameplay client at `/play/` with the soundtrack supplied by xu4, without local game
data, saves, debug tools or runtime fixture hooks. It uses a separate engine build directory
and does not overwrite the LAN engine. See `clients/site/README.md`.

Browser imports now verify complete supported English DOS/EGA profiles,
including the documented 1.01 data-fix variation:
103 configured maps/dialogues/images/executable files, sizes and SHA-256.
Other modified/translated data is not supported until another profile is reviewed.
One complete installation folder is selected without mixing in nested upgrade
files. Multiple complete installations require a narrower selection.
Unrelated files and embedded saves are excluded from new imports; use the
adventure manager to import saves separately. ZIP path/duplicate/CRC/resource
checks run in disposable staging; bad replacements leave the library intact.
VGA imports currently require the exact verified U4UPGRAD.ZIP archive.
See `docs/engineering/GAME_DATA_VERIFICATION.md` for scope and limitations.

This is the first engine-backed browser vertical slice for Ultimatum. The
polished HTML shell renders the real xu4 SDL canvas and reads live party,
inventory, spell, location, prompt, and message state from the C++ engine over
a small WebAssembly bridge. Direction buttons, keyboard commands, and the
conversation composer all feed the engine's existing input loop.
The world viewport also accepts taps for the same bounded safe-route and
approach/interact behavior used by the iOS client; the D-pad remains available
for explicit movement.

Run it locally:

```sh
cp ../../.env.example ../../.env.local   # once; fill in your own identifiers
npm run dev
```

Then open <http://localhost:4173>.

`npm run dev` prepares an ignored local copy of `dist`, injects the selected
test Supabase publishable configuration from `.env.local`, and then listens on
the development machine's network interfaces and prints a second URL
for phones and tablets on the same network. Keep the terminal running and open
that URL on the phone. If no cloud configuration is supplied, Accounts fail
closed while local play and saves remain available. The full SDL engine is
available below `/engine/`. Process environment variables override `.env.local`.

## WebAssembly engine build

The real C/C++ engine build uses Emscripten, SDL2, zlib, and a WebAssembly
build of libxml2. Toolchain versions are pinned in `toolchain/versions.env`.
On first use, the npm command installs the project-local Emscripten SDK,
CMake, Ninja, SDL2, and libxml2 under ignored `.cache` directories, then
compiles the engine:

```sh
npm run build:engine
```

The bootstrap script can also be run directly when diagnosing setup. Windows
requires Python 3, Git for Windows, and the Visual Studio 2022 C++ Build Tools;
macOS/Linux require Python 3 and a native C/C++ compiler. No global Emscripten installation is
used. Windows npm scripts enter Git Bash automatically and load the Visual
Studio developer environment automatically.

The engine requires a local copy of the Ultima IV DOS data. Import an existing
copy once, then rebuild:

```sh
npm run import:game-data -- /path/to/ultima4-data
npm run build:engine
```

With the local server running, open <http://localhost:4173>. The standalone
legacy SDL page remains available at
<http://localhost:4173/engine/ultimatum-engine.html> for debugging.

The imported data stays in the ignored `.cache/game-data` directory and is
bundled only into the ignored local build output. The local development save
is migrated once to Slot 1; the title waits for an explicit Continue or New Game
choice instead of automatically resuming it. Public builds disable that development preload and use the browser
importer; do not commit or redistribute the game data. The browser now
accepts an installed game folder, a local ZIP, or a direct URL to a
CORS-enabled ZIP and retains that selection in IndexedDB. Adventure slots use
their own transactional IndexedDB store. A selected checkpoint is validated
and copied into the engine's working filesystem before startup; game-data
re-imports cannot overwrite the slot records. Save success is reported only
after the checkpoint transaction completes.

## Title, new games and save management

The title offers three independent slots with Continue and New Game. Manage
reveals Import, Export, Rename, Delete, and Recover previous where available;
empty slots also offer Import directly. New Game uses
touch/keyboard-friendly name, character, original story and virtue-question
prompts; cancelling before the story publishes nothing. Replacing an existing
slot is confirmed, and its checkpoint is retained until creation finishes.

Menu → Adventures & saves pauses the engine while managing slots. Save current
adventure is available only where the original game permits saving. Automatic
checkpoints run every ten safe turns; suspension requests a best-effort queued
checkpoint. Save explicitly before closing: mobile browsers may suspend the
engine before background work can finish.

Exported `.u4save` JSON packages contain the original binary saves, any existing
journal/maps/pins/discoveries, version/game/engine identifiers and CRC32 integrity
checks. Import accepts this package or multiple classic save files selected
together; dungeon checkpoints require `OUTMONST.SAV` and `DNGMAP.SAV` too.
Native portable validators check every checkpoint before activation. Import
failures preserve good saves, and current/previous generations publish atomically.
Stale tabs cannot overwrite newer checkpoints. Failed durable writes retain
the last good slot and offer an unstored-checkpoint export before reloading.

Saves remain local to the browser origin/device and playable without an
account. Use the same address to return to them; clearing site data removes
them, so export external backups. Optional private cloud synchronization is
available through **Account & cloud saves** for both portable `.u4save`
checkpoints and separately selected game-data packages. The browser-local slot
store remains authoritative for offline play. Existing native-format journal,
exploration-map, pin, discovery, and dungeon-floor metadata is preserved and
editable on web.

Create a public bring-your-own-data engine without the development preload:

```sh
ULTIMATUM_BUNDLE_U4_DATA=OFF npm run build:engine
```

The build automatically includes a verified graphics-only VGA overlay, not a
destructive patch step. Compatible game-data imports need no separate VGA
upload. Classic/EGA choices and original game data/saves are preserved. The
legacy developer import command is still available:

```sh
npm run import:vga -- /path/to/u4upgrad.zip
npm run build:engine
```

Generated dependencies and engine artifacts stay under `.cache` and
`dist/engine`; neither is committed. Asyncify lets the engine retain its native
event loops while yielding to the browser. The browser-local slot store remains
the offline source of truth. Account-backed checkpoint and private game-data
transfers use the Supabase project supplied through ignored local or CI
configuration and the checked-in client contract. See [Accounts and cloud saves](../../docs/engineering/ULTIMATUM_ACCOUNTS_CLOUD_SAVES.md)
for quota, conflict, recovery, access-control, and deployment details.

## Menu and preferences

Open Menu in the desktop header or phone shortcut row. Explore and Travel
provide original commands without requiring a keyboard; browsing and Back
are free, while accepted game actions keep their original turn/resource costs.
Experience shares the iOS Classic/Ultimatum/Assisted preference schema, with
graphics/message overrides and confirmed Restore Profile Defaults. Controls
stores the three optional touch interactions separately. Exploration maps and
pins use the same engine-owned persistence as iOS and render in the web shell.

Local bundled-data builds enable Debug Tools for development. Disable them
explicitly with `ULTIMATUM_WEB_DEBUG_TOOLS=OFF npm run build:engine`; public
BYOD builds currently enable them for early-user testing and include the
graphics-only VGA overlay and the soundtrack supplied with xu4. Alternate
soundtrack packs and the soundtrack picker are excluded. The production
preflight requires this explicit early-access configuration. For the isolated
test-cloud package, run `npm run build:qa` in `clients/site`; it writes
`build-test/`. Gameplay corrections
from Pix's DOS patcher are a separate future audit, not part of asset import.
Adventure-changing tools require a pre-change classic-save snapshot where
saving is allowed, or explicit consent to continue without one. The current
session's latest snapshot is downloadable under Debug Tools → Diagnostics.
Its ZIP contains classic saves only, not journal/maps/pins or native slot
metadata; use Adventures & saves for full local checkpoint backups/recovery.

Runtime fixtures and generated test engines are isolated from the LAN client:

```sh
npm run test:runtime:build
npm run test:runtime:serve
```

The default fixture omits the soundtrack payload. Build the audio fixture with
`ULTIMATUM_BUNDLE_MUSIC=ON npm run test:runtime:build`; it includes the xu4
soundtrack and enables `?audio_suite=1`.

Open `http://127.0.0.1:4174/?menu_suite=1` for Menu assertions or
`http://127.0.0.1:4174/?suite=1` for special conversations. A single Menu case
can be selected with `&case=Make%20camp`. See
`docs/engineering/WEB_MENU_RUNTIME_RESULTS.md` for coverage and limitations.
Use `?walk_suite=1` for the eight real-engine tap-route regressions, or add
`&fixture=37` to isolate the first-step encounter. See
`docs/engineering/WEB_TAP_WALK_RUNTIME_RESULTS.md` for reproduction evidence.

Use `?hud_suite=1` to check stationary Wait/Search turns and stable feedback /
Menu geometry. `/responsive-runtime.html` runs that suite inside a real
393 × 700 frame viewport; `?layout=landscape` uses 844 × 390. The iframe
exercises the app's actual responsive CSS independently of host-browser
viewport overrides. Select `&suite=menu`, `&suite=walk`, or
`&suite=conversation` to run the existing regressions at those sizes.
Use `?onboarding_suite=1` for the full creation/save/backup/recovery lifecycle.
It uses test-owned slots on an isolated loopback origin, actual IndexedDB and
the real WASM engine. Do not run it against production/user browser storage.
Use `?parity_suite=1` for world-map/pin, dungeon-map/view, and combat-target
coverage. Use `?lifecycle_suite=1` to dispatch `pagehide`, reload the full
document like a pruned tab, and verify automatic authoritative-slot recovery.
`/responsive-runtime.html?suite=onboarding` exercises the phone layout.
These test pages are served only by the isolated fixture server, not the LAN
client. See `docs/engineering/WEB_HUD_RUNTIME_RESULTS.md` for evidence.

Rebuild the bundled account SDK with `npm ci && npm run build:cloud`. Versions and dependency licenses are pinned and checked in; no CDN scripts are required.

## Checks and test prerequisites

`npm run check` performs syntax checks without requiring a generated engine.
`npm test` runs the Node contract suite and loads the configured native compiler
environment on Windows. The generated-export assertion requires a completed
`npm run build:engine`; a fresh checkout intentionally has no ignored
`dist/engine` output. Runtime suites have their own isolated build and server
commands above and must not be run against a user or production origin.
