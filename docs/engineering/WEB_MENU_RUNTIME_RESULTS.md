# Web Menu runtime verification

Verified 2026-09-15 UTC with the actual Emscripten engine and shipped HTML
controls in the Codex in-app browser. Phone-sized browser viewports are not
physical iPhone Safari verification.

Reverified September 17, 2026 on Windows with the user's exact reviewed
English DOS/EGA archive in an ignored test cache. The complete desktop Menu
suite reported `SUITE PASS`; the rebuilt fixture included the automatic VGA
overlay. No original game files or test saves entered the repository or site
package.

## Evidence

| Run | Result |
| --- | --- |
| Phone portrait, 390 × 844 | All 53 Menu cases passed, followed by a real browser reload |
| Desktop, 1280 × 720 | All 52 original Menu cases passed, followed by reload; the added dungeon GEM case also passed separately with reload |
| Debug disabled, no VGA archive, optimized diagnostics-off engine | All four gating/preference cases passed, followed by reload |
| Special conversations, desktop | All 23 existing real-engine regressions passed on the final build |
| Special conversations, phone portrait | All 23 regressions passed after the modal layout changes |
| Automated unit/contract checks | 11 tests passed; JavaScript syntax checks passed |

The Menu suite clicks the real UI against isolated fixture-enabled engines,
not mocked gameplay. It checks world geometry when Menu opens, visible modal
bounds, blocked background mutations, disabled answers, stale and duplicate
submissions, resource/state changes, cancellation, and return to main controls.
Reload checks compare actual Experience/Controls values and persisted settings
against their pre-reload values, and verify session-only debug switches reset.

Covered flows include search, camp, quest-item cancellation and Horn use,
doors/unlocking/chests, torches, dungeon ladders, GEM maps, horses, cannon,
balloon ascent/landing, sextant, all three profiles, overrides/defaults,
interaction preferences, Debug categories, navigation/grants/world actions,
danger confirmation/cancellation, end combat, recovery ZIP bytes, and saving.
The normal resting branch and a random camp ambush were both observed. An
ambush is validated as real combat and cleaned up through the shipped Debug
End Combat action; its original combat turn accounting is not asserted to be
the same as peaceful camping.

Manual release-engine checks verified Enter and Space opening Menu, Enter
on Resume, reverse-Tab focus wrapping, and Menu → Game data → Close returning
to normal controls. Menu browsing/resuming did not spend a turn.
Desktop and portrait phone Menu presentation were inspected visually.
Landscape visual verification remains pending: the browser's viewport override
did not change the observed tab dimensions during this check.
Subsequent compact-HUD verification used an actual 844 × 390 frame viewport
to inspect landscape and check root Menu/Resume geometry. The full Menu suite
also passed at an actual 393 × 700 portrait frame size, including persistence
after reload; see `WEB_HUD_RUNTIME_RESULTS.md` for this later evidence.

## Runtime findings fixed

- Optimized Emscripten `ErrnoError` does not reliably contain diagnostic text.
  Existing-directory handling now checks actual filesystem existence/type;
  previously that error could bypass the IndexedDB save mount altogether.
- The modern HTML shell owns keyboard input independently of SDL's private
  canvas. Focused HTML controls activate through one click path, with modal
  focus containment and inert background controls.
- Menu and special prompts use visible temporary modals/sheets. A reserved
  message row prevents opening them from shrinking or moving the world view.
- Missing VGA data keeps EGA active without discarding the selected profile's
  desired theme or preventing unrelated preference changes.
- Dungeon GEM rendering distinguishes terrain and restores the previous
  view mode when dismissed.

## Reproduce

```sh
cd clients/web
npm test
npm run check
npm run test:runtime:build
npm run test:runtime:serve
```

Open `http://127.0.0.1:4174/?menu_suite=1` and inspect the visible result report.
For special conversations use `?suite=1`; to isolate a Menu flow use
`?menu_suite=1&case=Dungeon%20Gem%20Peer` (or another exact case name).

The gating run uses a separate test engine configured with
`ULTIMATUM_WEB_RUNTIME_TESTS=ON`, `ULTIMATUM_WEB_DEBUG_TOOLS=OFF`,
`ULTIMATUM_WEB_DIAGNOSTICS=OFF`, and an absent `ULTIMATUM_U4_UPGRADE` path.
Serve its output using `tests/serve-runtime.py --port 4175 --engine-dir PATH`
and open `http://127.0.0.1:4175/?menu_suite=1&debug_gate=1`.
This test-only build uses private fixture data; it is not a redistributable
public bundle. Normal builds have fixture exports disabled. Public BYOD builds
default Debug Tools off and preload neither original data nor the VGA archive.

## Remaining parity and verification

- Discovered exploration maps, remembered dungeon floors, and player pins
  need their real web surfaces. Their Experience choices are disabled.
- GEM is a temporary original-game full-area/current-floor view, not the
  exploration-map feature.
- Debug recovery ZIPs contain classic saves only. Native three-slot recovery,
  journal/maps/pins metadata, portable adventure bundles, and a first-class
  restore UI remain separate work. Download addresses the latest snapshot in
  the current session; it is not a cross-reload recovery browser.
- Physical iPhone Safari, Android browsers, controller navigation, storage
  quota/failure injection, and complete-adventure coverage remain pending.
- Newly added native D-pad sizing/handedness settings are outside this web
  slice and still need responsive web adaptation.
