# Web Journal and information framework — 2026-09-15

## Framework

- **Conversation:** the current exchange and its reply controls. The exact live
  prompt and unsent input survive a Journal visit. Creation/menu/service prompts
  use the same input surface, not the historical knowledge archive.
- **Log:** recent gameplay actions and feedback, including combat. NPC exchanges
  do not duplicate their full prose here. Compact latest feedback remains useful
  while commands are active.
- **Journal:** adventure-owned, already-revealed passages grouped by source;
  searchable text/people/places/topics; Current Clues bookmarks; attached notes
  and standalone My Notes. Earlier web transcripts remain read-only in Journal.

Journal is a paused deep-reading dialog: full-screen on phone-sized portrait
and landscape layouts, scrollable body, fixed Close, and at least 44px actions.
Bookmark and add/edit note use small inline SVG icons with accessible labels.
The mobile interface playbook's deep-view, cancellation, safe-area, and context
preservation guidance applies; no intentional deviations were needed.

## Data and persistence

The browser engine now records presented NPC/vendor dialogue and shrine advice
using `TopicJournal`. It never feeds unread dialogue or puzzle catalogues into
discovery. Native passage metadata and identity are retained; imported native
passages of other kinds (including writings) are readable without new reveal
hooks or pre-reading game files.

`topics.txt` and `journal-notebook.dat` use the same models/formats as iOS.
Bookmarks use the native full passage identity; note identifiers are strings in
JSON to avoid losing 64-bit precision. Notes never teach unseen game vocabulary.
Limits remain 256 bookmarks/notes and 4,000 UTF-8 bytes per note.

Done saves notebook edits through a journal-only IndexedDB transaction in the
active slot. Gameplay bytes, save timestamp/summary, and the previous recovery
checkpoint are unchanged. Stale-tab protection still applies. Durable failures
restore MEMFS and the native notebook while retaining the unsaved editor draft.
IDBFS is a best-effort working-copy mirror, not the authoritative slot store.
Revealed passages also travel with ordinary gameplay checkpoints; save before
leaving to retain newly encountered material that has not yet been checkpointed
or included in a bookmark/note transaction.

Emscripten's `fsync` unwinds asynchronously. The web build deliberately omits
the native disk flush inside `JournalNotebook::save`, retaining atomic MEMFS
rename and using the browser transaction for durability. This prevents re-entry
into Asyncify while the engine awaits an event. Native/iOS disk flushing is
unchanged.

Save whitelist, export/import, and native portable validation now include the
notebook. Legacy saves without it remain valid; malformed files and bookmarks
referencing absent passages are rejected before activation. Cloud sync and the
iOS portable-package import UI remain separate work.

## Verification

- `clients/web`: syntax checks and all **30 unit/contract tests passed**.
- `clients/site`: syntax checks and all **8 packaging/deployment-policy tests
  passed**, including Journal shell/module, native matching-source inclusion,
  and exclusion of runtime fixtures from release output.
- Native host mobile suite: all **14 model/rules/storage groups passed**, plus
  both VGA overlay checks. iOS behavior has only web-conditional changes here;
  no new native UI or device installation was required.
- Real engine and real Journal DOM controls on disposable origins
  `127.0.0.1:4208` and `:4209`: Introduction/Name recordings, bookmark toggles,
  attached Unicode/quoted/multiline notes, standalone notes, search, UTF-8 bounds,
  Cancel, Keep/Delete confirmation, injected durable-write failure rollback,
  exact live prompt/draft restoration, Goodbye/main controls, historical
  transcripts, and paused exploration/conversation timers all passed.
- Byte-exact export/decode and real-engine native validation passed; legacy
  notebook omission loads; malformed/dangling notebook data is rejected.
  A separate imported slot survived reload unchanged. Final world checkpoint
  and Continue restored favorites and notes in the actual running engine.
- Direct UI checks at **375×667 portrait** and **844×390 landscape**: full-screen
  Journal, Current Clues without freezing, readable passage/attached-note text,
  compact finger-sized icons, scrollable body and visible Close. Temporary
  viewport override was reset. Real iPhone Safari/virtual-keyboard coverage is
  still outstanding; desktop Chromium-based browser testing is not that claim.

The LAN artist-preview package in `clients/site/build` was rebuilt with the new
core/shell and remains bring-your-own-game-data. Existing user saves and game
data were not edited: runtime QA used disposable origins. No public deployment
or commit was performed.
