# iOS journal notebook — 2026-09-15

## Delivered behavior

Places remains the default location/person journal. Current clues bookmarks
individual revealed passages and returns to the original continuous transcript.
My notes contains attached and standalone personal writing, with search,
Done/Cancel, stable editing, and explicit delete confirmation. Unbookmarking
preserves the attached note; deleting a note preserves the source and bookmark.
The journal uses a world-visible sheet with explicit list/transcript paging.
The native editor owns keyboard input; journal browsing freezes world timers
and rejects queued gameplay keys/actions. Close restores ordinary controls.

Notebook metadata is separate from TopicJournal discovery. Writing does not
alter NPC dialogue, teach keywords, or reveal undiscovered clues. Passage keys
use length-prefixed source/speaker/topic/text identity, not array indices.
`journal-notebook.dat` is atomically saved in the selected adventure directory
on each committed edit, carried by checkpoints, and validated during recovery.
Missing metadata is an empty legacy notebook. Corrupt metadata is preserved;
editing/checkpoint publication fails safely rather than silently erasing it.
Limits are 256 bookmarks, 256 notes, and 4,000 UTF-8 bytes per note.

## Evidence

- All 14 portable suites pass, including the new notebook suite: stable passage
  references, Unicode/multiline/quoted text, edit/delete, unbookmark preservation,
  discovery separation, limits, failed writes, corrupt-load rollback, and
  distinct checkpoint notebooks/recovery.
- Production Simulator Release build succeeds.
- Opt-in `org.ultimatumproject.tests.journal` runs the actual engine and native views
  using an isolated fixture on iPhone 17 Pro Simulator. PASS output covers
  default Places, world visibility, 44-point tabs/New note, real bookmark
  persistence/context, attached/standalone notes, native Done/Cancel editing,
  failed-save draft retention, deletion, note search, no turn/resource cost,
  actual world-timer freezing, keyboard/control release, and checkpoint saving.
- A real engine termination/relaunch passes note and bookmark restoration and
  repopulates both native notebook modes.
- Standalone UIKit tests load no saves. All three journal modes fit complete
  paginated rows and retain 44-point paging/New note/passage action targets on
  iPhone SE (375×667 portrait) and iPhone 14 Pro (852×393 landscape).
- Portrait Current clues and reloaded My notes screenshots inspected. Fixed
  low-contrast tabs and a narrow Previous label before final device signing.
- Signed physical-device Release build 13 succeeds (version 1.0).
- Installed over the configured development bundle on the connected iPhone without
  uninstalling its data container, launched successfully, and verified from
  device app inventory as version 1.0 / bundle version 13. The preview bundle
  is untouched.

Logs/screenshots are local temporary evidence:
`/private/tmp/u4-journal-runtime-build.log`,
`/private/tmp/u4-journal-final-clues.png`,
`/private/tmp/u4-journal-my-notes.png`, and
`/private/tmp/u4-build13-final-device.log`. Runtime console files are under each
Simulator device's `data/tmp/` directory.

## Coverage limits

The Mac was locked and computer-use access could not unlock it. Automated
engine/native-panel integration and layout assertions are not finger-touch
coverage. Physical touch typing, interactive keyboard resizing, interruption
with an unsaved draft, VoiceOver, very large accessibility text, and iPad remain
manual/runtime coverage gaps. No production adventures were used for tests;
simulator fixture edits occurred only in the separate journal test bundle.

No TestFlight/App Store upload is authorized or performed.

## Build 14 follow-up: reported Current clues freeze

The user reported a freeze when tapping Current clues and questioned the smaller
journal pane. Restored the full-screen safe-area card in both orientations. This
is intentional project-specific use of the playbook's full-screen modal pattern
for a deep, paused reading system; the earlier world-visible layout was not a
requested journal change.

Replaced UISegmentedControl with the same plain-button family used by existing
menus, and deferred table/view hierarchy updates until after button dispatch.
This addresses a suspected native tracking/nested SDL event-loop interaction,
not a confirmed stack-trace diagnosis. Original tests directly switched modes
and did not establish that the segmented control responded to physical touches.
An attempted phone CPU trace could not capture the reported stuck process: it
was no longer available when the profiler resolved the physical device.

Added actual UIControl target-dispatch regressions: 18 repeated mode changes
with both empty and populated Current clues, note-list correctness, and return
to main controls. These pass in the isolated real-engine fixture, alongside the
existing note/persistence tests and all 14 portable suites. Standalone layout
assertions now require a full-screen card and 44-point mode buttons. Actual
finger-touch confirmation is still pending while the Mac remains locked.

The updated full-screen layout passed on iPhone 17 Pro portrait (real-engine
fixture) and iPhone 14 Pro landscape (standalone UIKit). The extra iPhone SE
rerun stalled during Simulator startup/install and was stopped; the build-13
small-phone evidence does not establish build-14 coverage. Signed device
Release build 14 succeeds. No unrelated worktree changes were reverted.
Installed over the existing phone app without uninstalling, launched
successfully, and verified from device inventory as version 1.0 / build 14.

## Build 15: compact passage action icons

Replaced the two full-width passage actions with native SF Symbols in the
upper-right heading row: `bookmark` / `bookmark.fill` and `square.and.pencil`.
An attached personal note adds a small `checkmark.circle.fill` badge. Both
actions retain separate 44×44-point hit areas, saved-state VoiceOver values,
and explicit action labels. Failed bookmark writes display a warning symbol
and announce failure without changing the saved bookmark. The journal remains
full-screen; source text, note persistence, paging, and plain-button tabs are
unchanged. No new external artwork or raster asset is needed.

All 14 portable suites pass. The isolated real-engine fixture passes native
icon image/size/state/badge checks, actual UIControl target dispatch for
bookmark removal/restoration, attached-note preservation, existing-note editing
and cancellation, empty add-note draft/cancellation, and the prior notebook
and repeated mode-transition tests. Portrait screenshot inspected at
`/private/tmp/u4-journal-icons.png`. Standalone iPhone 14 Pro landscape layout
checks pass for full-screen sizing, icon-only passage actions, 44-point targets,
and accessibility labels. Signed Release build 15 succeeds. Physical touches
and VoiceOver navigation remain untested because the Mac is locked.
Installed over the existing phone app without uninstalling and verified from
device inventory as version 1.0 / build 15. A transient disconnection resolved
on retry; automatic launch remains pending because the phone is locked.
