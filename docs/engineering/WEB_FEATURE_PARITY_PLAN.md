# Web feature parity plan

## Goal and definition of parity

The web client should be able to complete the same Ultima IV adventure as the
iOS client while preserving the same game rules, turn accounting, save data,
and player choices. Parity means equivalent gameplay capability and state, not
identical screen geometry. Desktop web, mobile web, and native iOS may compose
the same semantics differently.

Parity is reached when:

- every required game flow has a discoverable touch path and a keyboard path;
- neither web UI implements game legality or turn rules that belong in C++;
- an adventure can be saved, closed, restored, exported, and moved between iOS
  and web without losing engine or Ultimatum metadata;
- the title sequence, character creation, exploration, conversation, party and
  equipment, magic, combat, dungeons, shrines, vendors, and ending are covered;
- portrait phone, landscape phone, tablet/desktop, keyboard, touch, and basic
  controller navigation have explicit verification; and
- accessibility, interruption, storage failure, and recovery states have safe
  behavior rather than falling back to the legacy SDL interface.

## Architecture that keeps the clients in sync

```text
Ultima IV / xu4 C++ engine
        |
Versioned semantic client contract
  state snapshots + intent commands + save packages
        |
        +--------------------+
        |                    |
iOS native adapter     WebAssembly EngineClient
        |                    |
iOS UIKit surfaces     Desktop + mobile web surfaces
        |
Local SaveStore -------+------ future Ultimatum Account SaveStore
```

The semantic contract is the product boundary. It describes player intent
(`activatePrimaryAction`, `moveToTile`, `selectCombatTarget`) and observable
state (`primaryAction`, prompt type, party, inventory), rather than exposing
raw key presses as the long-term API. The engine remains authoritative for
availability, validation, mutations, and turn cost. Clients render and submit
intent.

The JavaScript `EngineClient` is the web-side boundary. Browser layout code
must not call WebAssembly functions directly. New contract fields should be
additive and versioned so a future worker-hosted engine or cloud restore does
not require rewriting the UI.

## Delivery sequence

### 0. Contract foundation — in progress

- Publish a versioned snapshot schema and semantic action model.
- Move reusable iOS gameplay semantics behind platform-neutral C++ entrypoints.
- Add capability flags so a UI can disable unavailable features honestly.
- Keep raw key input only as a compatibility and desktop-keyboard path.
- Add contract tests around every exported intent.

First slice: expose the already-tested shared current-tile resolver to the web
client and replace the fixed Enter button with the live contextual action.

Delivered slices: versioned snapshot/capabilities, live current-tile and unique
adjacent interaction actions, and the JavaScript `EngineClient` boundary.

### 1. Exploration and conversations

- Share safe tap-to-walk, approach-and-interact, cancellation, and stale-target
  validation with WebAssembly.
- Map canvas taps to the visible 11-by-11 tile grid and preserve the D-pad as an
  accessible alternative.
- Represent direction, choice, player, amount, letter, confirmation, and free
  text prompts semantically.
- Build stable conversation controls, topic history, vendors, donations,
  companion recruitment, Lord British, shrines, and quest-item interactions.

Delivered slice: the iOS safe-route, approach/interact, stale-target, hazard,
and cancellation behavior now compiles into WebAssembly and canvas taps submit
tile-relative intent through `EngineClient`. The isolated tap-route suite now
verifies multi-square movement, retargeting, cancellation, door/NPC approach,
Goodbye, and encounter handoffs. Route startup and delayed steps are SDL-owned
to avoid re-entering Asyncify during a yielding turn. Full hazard/stale-target,
complete-adventure, and physical-device coverage remain before this portion
is considered complete; see `WEB_TAP_WALK_RUNTIME_RESULTS.md`.

Delivered slice: free text and amount prompts now submit a complete value,
while choice, letter, and party-member prompts submit exactly one selection.
The bridge identifies the active engine controller, preventing a synthetic
Enter key from leaking into the next conversation state.

Delivered slice: the responsive conversation dock now uses stable layouts for
topic entry and compact prompts, fits all offered controls without covering the
command bar, and locks unrelated commands while an answer is pending.

Implemented slice: special conversations use engine-owned modal prompts with
full reply text, named vendor goods/services, buy/sell and food/ale choices,
quantities and offers, service recipients, NPC yes/no questions, donations,
companion recruitment, Lord British healing confirmation, Hawkwind virtues,
and shrine virtue/cycle/mantra/vision controls. Shop quantity cancellation
terminates the pending transaction; donation cancellation returns to topics
without donating; cancelling a mantra leaves the shrine. Hidden vendor topics
and mantras are not revealed by the UI. Mobile prompts use a temporary sheet
rather than adding permanent HUD rows.

Answers are queued atomically through the SDL loop, tagged with a prompt
generation, validated by the active controller, and accepted at most once.
The C++ controller execution test covers named choices, numeric validation,
cancellation, keyboard entry, continuation, and duplicate submission. An
isolated real-engine browser fixture suite now exercises these special flows;
coverage and reproduction instructions live in
`WEB_CONVERSATION_RUNTIME_RESULTS.md`. Runtime testing found and fixed lost
Continue values and premature command activation during healing animations.
Full adventure/device coverage and rejected-service branches remain before
this stage is considered parity.

### 2. Party, inventory, equipment, magic, journal, and maps

- Expose complete inventory, reagents, mixtures, equipment legality, and party
  ordering through read models and intent commands.
- Add party details, weapon/armour changes, spell mixing, casting parameters,
  recipients, and clear cancellation behavior.
- Port the location-first journal, discovered-place map, dungeon discoveries,
  and player-created pins as temporary panels.
- Keep browsing free; let the engine report when an accepted action spends a
  turn or changes resources.

Delivered slice: party cards become explicit member choices while the engine
is awaiting a player, and spell buttons now wait for the engine's letter prompt
instead of relying on a timing delay. Unmixed or currently illegal spells are
disabled from live prerequisite state.

Delivered slice: party members now open an engine-backed equipment detail.
The snapshot supplies shared weapon/armour inventory and legal choices for
each member; semantic equip intents are revalidated in C++ and spend a turn
only after a successful change. Combat permits only the active fighter's
weapon, matching the native client.

Delivered Journal slice: revealed NPC/vendor passages and shrine visions now
use the native `TopicJournal` model and appear in a paused responsive Journal.
Current Clues bookmarks, attached notes, standalone notes, search, historical
web transcripts, and native-format notebook persistence are implemented.
Journal-only durable edits retain the previous gameplay checkpoint and reject
stale writers. Closing the Journal restores the live prompt and unsent reply.
Exploration-map, discovered-place, pin, and dungeon-discovery UI are now shipped
as separate parity surfaces. Runtime evidence: `WEB_JOURNAL_RESULTS.md` and
`WEB_PARITY_LIFECYCLE_RESULTS.md`.

### 3. Combat and dungeons

- Expose active combatant, legal movement, visible targets, selection, weapon
  range preview, attack confirmation, spell targeting, chests, and fleeing.
- Replace exploration controls with a dedicated battle composition.
- Add relative dungeon movement, torch/search, ladders, 3D/overhead switching,
  line of sight, and remembered floor maps.
- Verify that delayed input, cancellation, and orientation changes never spend
  unintended turns.

Delivered mobile composition: phone web now uses the same control hierarchy as
the native client rather than stacking a context bar above generic commands.
Combat maps Prev/Next/Clear/Attack into the shared 2×4 action deck; dungeons map
Torch/context action/view/Search into those slots. Search appears once, the
floor map remains reachable from the live minimap, and recent feedback is a
three-line in-world overlay with full history still available in Log. The real
fixture engine passes world-map/pin, dungeon view/floor-map, combat selection/
clear, touch-target, Menu cancellation, and stable-geometry checks at a 393 ×
852 phone viewport.

### 4. Saves, recovery, and portable adventures

- Mount the web save directory on durable browser storage and synchronize it
  after successful saves and before suspension where the browser permits.
- Port the three-slot checkpoint model, validation, `CURRENT`/`PREVIOUS`
  recovery, periodic checkpoints, and clear storage-error feedback.
- Define a versioned `AdventureBundle` containing classic save files plus
  journal, explored maps, pins, discoveries, slot metadata, hashes, and engine
  compatibility information.
- Prove iOS → web → iOS round trips using fixtures before connecting the
  existing cloud-sync service to this client.

Delivered slice: the title selects among three transactional IndexedDB slots.
Legacy working saves migrate once into Slot 1; the engine activates only a
validated selected checkpoint. Rename, confirmed replacement/delete, package
and classic-file import, export, validated previous-checkpoint recovery, periodic
safe-turn checkpoints, stale-tab guards and emergency export are implemented.
Each slot atomically retains its current checkpoint and immediate predecessor.
`.u4save` packages preserve classic binaries and existing native-format optional
metadata with version/game/engine identifiers and CRC32 integrity checks.
Native iOS Files import/share export now uses the same version-1 format. Native
codec → web edit/save → native validation/relaunch, web export → native install,
and user-confirmed iOS → mobile web → iOS round trips have passed. The
platform's existing opt-in cloud save/game-data capability still needs its
versioned provider and identity contract wired into this checkout;
physical-browser interruption coverage and third-party document-provider
downloads also remain separate work. Runtime evidence:
`WEB_ONBOARDING_SAVE_RUNTIME_RESULTS.md` and
`IOS_ADVENTURE_PACKAGE_RESULTS.md`.

### 5. Onboarding, preferences, accessibility, and release coverage

- Give title, new/journey-onward, three-slot selection, story, virtue choices,
  and character naming first-class responsive controls.
- Add audio controls, UI scale, handedness, interaction preferences, reduced
  motion, keyboard/controller mappings, and screen-reader state.
- Exercise the complete critical path on Safari iPhone, Safari/Chrome desktop,
  and at least one Chromium Android browser.
- Maintain a fixture-driven parity matrix and record runtime evidence; a
  successful build alone does not close a feature.

Delivered onboarding slice: explicit title/Continue/New Game with three slots,
name and character choices, original story continuations and virtue questions
use responsive semantic controls. Cancellation before the story returns to
title without publishing; a new adventure does not replace another slot or
skip the original class/virtue calculation. Physical Safari/Android and broader
controller/accessibility coverage remain pending.

## Current parity matrix

Delivered Menu slice: desktop header and mobile shortcut open engine-owned
Explore, Travel, Experience, Controls, Save journey, and Game data surfaces.
Quest items have named owned choices and free cancellation. Door/chest,
torch/camp, ladder/balloon, transport/horse/cannon, sextant, and gem commands
reuse original engine legality, resource changes, and turn accounting. Peer
has a temporary full-area/floor GEM map; it does not discover terrain or add
player pins. Modal prompts reserve the message row behind them, are visible
in shorter desktop windows, isolate background controls, and retain keyboard
focus with an explicit Resume/Back path.

Experience uses the existing shared Classic/Ultimatum/Assisted preference
schema, clears individual overrides only after confirmation, shows Customized,
and supports immediate graphics and movement-message overrides plus Restore
Profile Defaults. Missing VGA files leave EGA active without erasing the
desired profile theme. Exploration-map and pin choices are live on web and iOS.
Controls persists bump, direct interaction, and tap-walk preferences separately
from Experience. Native D-pad sizing/handedness and other release preferences
remain future web work.

Developer Debug Tools are compile-time gated and currently enabled in public
BYOD builds to support early-user testing. They expose session switches, navigation, grants, world tools,
diagnostics, and separately confirmed destructive actions. Adventure-changing
tools require a pre-change classic-save snapshot where saving is allowed;
otherwise the user explicitly accepts proceeding without one. Diagnostics can
download the current session's last recovery snapshot as a classic-save ZIP.
This is not the native three-slot/Ultimatum-metadata recovery model. Runtime
evidence and reproduction live in `WEB_MENU_RUNTIME_RESULTS.md`.

| Capability | Web state | Next proof |
|---|---|---|
| Engine, rendering, audio | Playable slice | Longer mobile-browser session |
| Game-data/VGA import | Local folder/ZIP/URL | Public BYOD build and failure matrix |
| Context action | Engine-backed UI | Runtime labels and turn-behavior matrix |
| Movement | Shared tap routes, cross-shaped touch D-pad, keyboard | Safari/Chrome session and interruption matrix |
| Conversations | Named special-flow prompts and atomic/stale-safe answers; isolated real-engine flow regression suite | Rejected services, remaining quest branches, physical Safari/Chromium coverage |
| Party | Status, prompt-time selection, equipment inventory and changes | Attributes, supplies, ordering |
| Magic | Live legal spell list and prompt-synchronized casting | Mixing and semantic parameters |
| Combat | Engine-authoritative target list, selected/prepared/repeat state, target overlays and semantic cycle/clear/attack controls | Broader weapon/spell/encounter matrix and physical touch browsers |
| Dungeons | Semantic search/torch/view controls, directional labels, minimap and explored floor map | Broader room/trap/ladder and physical-browser matrix |
| Journal and maps | Journal, Current Clues, notes, search, historical transcripts, minimap, world exploration, discoveries, pins and dungeon floors with native-compatible persistence | Physical Safari/Chromium and cross-client map/pin round trips |
| Menu commands | Engine-owned Explore/Travel, quest items, temporary GEM view | Remaining command-legality branches and physical browsers |
| Experience and Controls | Shared profiles, graphics/message/map/pin overrides, restore defaults; persistent interaction preferences | Native D-pad size/handedness and broader audio/accessibility preferences |
| Debug Tools | Developer-only categories, confirmations, classic recovery snapshot/download | Broader unsafe-context and recovery-failure matrix |
| Saves | Transactional three-slot manager, validation/recovery, lifecycle checkpoints, tab-prune reload restoration, `.u4save` adapter and verified cross-client round trips | Bind the existing external cloud provider; physical-browser interruption/document-provider matrix |
| Character creation | Responsive title/name/character/story/virtue prompts with original engine rules | Physical Safari/Android, controller and broader accessibility coverage |
| Accessibility/controller | Basic HTML/keyboard | Full focus, announcements, remapping |

## Ultimatum Account seam

Account work should follow portable local saves rather than precede them. The
client will depend on a `SaveStore` interface, with browser-local storage as the
first implementation and the existing authenticated cloud capability bound
through a versioned provider contract. Cloud
sync should upload immutable adventure generations and choose a winner or ask
the player on conflict; it should never merge binary save files.

Game data is a separate `GameDataStore` concern. If accounts later include a
private user-provided allowance, game data and adventure saves must have
separate retention, quotas, deletion, and legal/privacy policy. Gameplay must
continue offline and must never require an account.

## Working discipline

- Land coherent vertical slices with their contract and tests together.
- Update this matrix and `WEB_CLIENT_STATUS.md` whenever a slice changes status.
- Prefer small additive contract changes over client-specific shortcuts.
- Validate engine behavior, responsive composition, and persistence separately.
- Do not mark a row complete until its expected behavior has runtime evidence.
