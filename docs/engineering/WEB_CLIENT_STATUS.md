# Web client status

## Engine-backed vertical slice

`clients/web` contains the first playable browser client for Ultimatum. Its
responsive HTML shell presents the world, conversation output, commands,
party, spells, journey log, and Journal while the real xu4 C/C++ engine runs in
WebAssembly behind it.

The current slice supports:

- live location, party, spell, prompt, message, and resource state projected
  from the engine through a small C++ bridge;
- keyboard and touch commands, safe tap-to-walk and approach/interact, semantic
  text/amount/choice prompts, engine-authoritative touch choices and direction
  controls, prompt-synchronized spell and party-member selection, graphics
  switching, and saving;
- a stable mobile conversation composer with fitted topic and direction
  controls, prompt-time command locking, and contextual actions for both the
  current tile and one unambiguous adjacent person or door;
- engine-owned special conversation prompts for vendor goods/services,
  quantities/offers, service recipients, donations, recruitment, NPC yes/no
  questions, Lord British, Hawkwind, and shrine meditation/visions; answers
  are atomic, generation-checked, and duplicate-safe, with mobile sheets and
  explicit cancellation; isolated real-engine browser regression coverage is
  documented in `WEB_CONVERSATION_RUNTIME_RESULTS.md`;
- party-member equipment details with live shared weapon/armour inventory,
  engine-filtered choices, combat restrictions, and semantic equip intents;
- an adventure-owned Journal of revealed NPC/vendor passages and shrine visions,
  grouped/searchable by source, Current Clues bookmarks, attached and standalone
  personal notes; iOS-compatible notebook saves, metadata-only durable edits,
  full-screen mobile reading, paused world timers, and return to the same live
  conversation prompt/draft; Conversation contains the active exchange and Log
  recent gameplay feedback, with historical transcripts retained in Journal;
- engine-owned Menu on both web layouts, with Explore and Travel commands,
  named quest items, free browsing/cancellation, temporary GEM maps, and
  original turn/resource rules;
- Classic, Ultimatum, and Assisted Experience profiles, confirmed override
  resets, graphics, movement-message, exploration-map, and pin customization,
  plus persistent bump, direct-interaction, and tap-walk preferences;
- a live 33×33 minimap, full adventure-owned 256×256 exploration map,
  discovered places and player pins, plus per-floor dungeon exploration maps;
- semantic dungeon controls and combat target overlays/cycling/clear/repeat
  state shared with the iOS engine boundary;
- developer-only Debug Tools, including session switches, navigation, grants,
  world tools, diagnostics, and confirmed destructive actions; pre-change
  classic saves can be downloaded for recovery, with explicit consent when
  the current context cannot save;
- original game-data import from a folder, a local ZIP, or a CORS-enabled ZIP
  URL, retained in the browser with IndexedDB; complete supported English
  DOS/EGA files are checked against a 103-file size/SHA-256 profile before
  publication, with bounded ZIP staging and failed-replacement recovery;
- a responsive title with three-slot Continue/New Game, semantic character
  naming/selection, the original 24-page story and seven virtue questions;
- transactional browser-local slots, rename/delete/import/export, validated
  current/previous recovery, automatic safe-turn checkpoints, legacy migration,
  stale-tab protection and emergency export after failed durable writes;
- versioned `.u4save` backups containing classic binaries and native-compatible
  journal/maps/pins/discoveries, validated before activation; native iOS Files
  import/share export supports the same format;
- an automatic checksum-verified graphics-only VGA overlay after validated
  original-data import, with no patch-upload control and retained EGA choices;
- the soundtrack supplied with xu4, with independent music/effects volume and
  credits; alternate soundtrack packs and their selection UI are excluded;
- a private 320x200 SDL canvas for engine compatibility and a stable 176x176
  world view copied directly from engine pixel memory into the modern shell;
- responsive desktop and fallback phone layouts, including mobile-safe tap
  behavior;
- a purpose-built mobile-web shell aligned to the native iPhone composition:
  a compact status/minimap header, a width-dominant world, a non-interactive
  three-line in-world log, lower-left D-pad, and one contextual 2×4 action
  deck for exploration, combat, and dungeons; duplicate command rows are
  removed, while Party/Spells retain the full Log bottom sheet and Game data
  remains available from the sheet footer;
- lifecycle-safe background/pagehide/freeze checkpoints, BFCache resume,
  active-slot recovery after browser tab pruning/reload, persistent-storage
  requests, and a PWA/offline shell service worker; and
- LAN serving for testing from phones and tablets on the same network.

## WebAssembly engine

The complete xu4 engine builds with Emscripten and a pinned SDL2 port, zlib,
and a static WebAssembly build of libxml2. Asyncify allows its synchronous SDL
event loops to yield to the browser. Web-only defaults keep rendering at the
native scale, reduce initial audio levels, and use a larger audio buffer to
avoid mobile-browser underruns.

The reproducible toolchain contract lives in
`clients/web/toolchain/versions.env`: Emscripten, its emsdk revision, CMake,
Ninja, SDL2, and libxml2 are pinned and installed below the ignored web cache.
The npm entry points work on macOS/Linux shells and on Windows through Git Bash;
Windows builds discover the Visual Studio C++ developer environment for the
native audio renderer and C++ contract test. The checksum-pinned audio inputs
are rendered through FFmpeg before a full public build.

Local development may bundle ignored Ultima IV data and a VGA overlay into the
ignored `dist/engine` artifacts. Hosted builds must disable original-data preload and
remain bring-your-own-data; original game files and generated engine bundles
are not committed.

The Node unit/contract suite verifies backup integrity/limits, metadata-only journal transactions,
compatibility profiles, the bridge surface, shell wiring, mobile rendering
and audio defaults, local data packaging, and generated WebAssembly exports.
The special-prompt test compiles and executes the real C++ prompt controller
with stubbed event-loop wait hooks; it covers named goods, quantity validation,
cancellation, keyboard entry, continuation, and duplicate submission. It
complements the isolated real-engine browser suites, which verify actual
resource mutations, cancellation, stale/duplicate answers, and return to main
controls. Interaction lifetime now includes yielding animations, preventing
commands from becoming active before services finish. Complete-adventure and
physical-device/browser coverage remain pending.
Menu evidence is recorded in `WEB_MENU_RUNTIME_RESULTS.md`.
Onboarding/save lifecycle evidence is recorded in
`WEB_ONBOARDING_SAVE_RUNTIME_RESULTS.md` (17 checks each on portrait phone,
landscape phone and desktop layouts, including real storage and injected
failure recovery).
Release-mode
testing found and fixed directory checks that depended on diagnostic error
text and could skip the IndexedDB save mount in optimized builds. The modern
HTML shell also owns keyboard input independently of SDL's private canvas.
Tap-route startup and delayed steps are queued onto the SDL-owned engine loop,
so a turn that yields for creature animation, combat, or dialogue cannot
re-enter Asyncify from a browser call. Targeted real-engine coverage is recorded
in `WEB_TAP_WALK_RUNTIME_RESULTS.md`.

## Public presence — test and production deployed

`clients/site` contains the Ultimatum Project homepage and a separate public
release at `/play/`. It excludes original game-data preloads, development
parties and fixture hooks; matching engine source and library
licenses are included. Engine assets are chunked for Cloudflare Static Assets.
The separate `test.ultimatumproject.com` Worker/custom domain has been deployed
and is currently publicly reachable as recorded in `clients/site/CLOUDFLARE_SETUP.md`.
The production apex and test origin both serve compile-gated Debug Tools during
early-user testing, while retaining separate cloud projects. The repository contains the Supabase
account/provider wiring for passwordless sign-in, immutable checkpoint sync,
recovery history, conflict handling, and separately initiated private game-data
upload under a shared server-enforced 100 MiB account allowance. Deployment
versions and any temporary client/backend environment sharing are recorded in
`clients/site/CLOUDFLARE_SETUP.md`.
The dated local release evidence in
`PUBLIC_SITE_RUNTIME_RESULTS.md` predates that test deployment; data verification
and the future private-deduplication boundary are described in
`GAME_DATA_VERIFICATION.md`.

The importer now selects a single complete installation directory without
mixing nested upgrade data and supports the documented English DOS/EGA 1.01
data-fix variation. Exact-archive runtime evidence is in
`WEB_GAME_DATA_IMPORT_RUNTIME_RESULTS.md`. The current original-data
redistribution evidence is recorded in `ULTIMA_IV_REDISTRIBUTION_NOTE.md`;
no original game payloads have been added to the public build.
The browser UI talks through an `EngineClient` boundary so the current
main-thread engine can later move to a worker without rewriting either layout.
The staged route to gameplay parity and the account seam are tracked in
`WEB_FEATURE_PARITY_PLAN.md`. Work has begun on the semantic action contract by
sharing the iOS current-tile resolver and safe routing behavior with the web
build and projecting semantic actions through `EngineClient`. Local data and
durable saves remain authoritative and usable without an account. Signed-in
clients can additionally synchronize validated saves and, through a separate
explicit action, private game-data packages.
