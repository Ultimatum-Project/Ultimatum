# Mobile regression checks

On macOS with the Xcode command-line tools installed:

```sh
./tests/run-mobile-tests.sh
```

On Windows, use the repository-owned WSL bootstrap. It installs a separate
Ubuntu development distribution and its Clang, CMake, Ninja, and zlib packages;
it does not modify Docker's internal distributions:

```powershell
.\vendor\ultima4-ios\tests\bootstrap-windows-native-tests.ps1
.\vendor\ultima4-ios\tests\run-mobile-tests.ps1
```

The bootstrap is idempotent. If Windows has just installed or updated WSL, a
restart may be required before the first package installation.

The runner builds in a temporary directory and runs the following suites:

- `experience`: profile defaults, overrides, migration, validation, and atomic persistence.
- `vga`: asset precedence in ZIP and loose-file installations, EGA/VGA switching, and original executable preservation.
- `topics`: exposure-only discovery, exact puzzle words, journal persistence/reset.
- `notebook`: passage bookmarks, attached/standalone notes, Unicode text, corruption/limits, atomic persistence, and checkpoint isolation/recovery.
- `rules`: mixing capacity and resource boundary checks without mutation.
- `combat`: unique enemy identities, per-fighter last attacks, weapon validation, and presentation delay limits.
- `map-pins`: exploration-gated player notes and persistence.
- `map-discoveries`: exposure-only place markers and persistence.
- `dungeon-exploration`: visited floor cells and metadata validation.
- `dungeon-sight`: remembered first-person dungeon visibility.
- `native-session`: versioned pause, checkpoint, quiesce, resume, shutdown, duplicate-event, and inactive-input policy.
- `recovery`: validated previous-checkpoint routing and rollback.
- `snapshots`: complete-generation publication and unsafe pointer rejection.
- `save-store-contract`: stable generation identity, compare-and-swap publication, and recovery.
- `slots`: active-slot persistence, legacy Slot 1 compatibility, and isolated snapshot roots.
- `adventures`: engine party/creature serialization with snapshot selection and journal isolation.

These checks do not launch UIKit, run the full game controller, validate dungeon
terrain restoration, or establish end-to-end completion. Simulator/device builds
and interaction tests are separate gates. Current fixtures use macOS `/private/tmp`.

Run `sh tests/run-adventure-package-tests.sh` separately for the actual native
Foundation codec, integrity and native metadata validation, legacy migration,
independent slot publication/recovery, malformed transport/moon rejection, and
native → web codec edits → native round trips (both world and dungeon saves).
`ZU4_PACKAGE_FIXTURE_DIR=/explicit/test/dir` additionally emits `world.u4save`.
The opt-in `ZU4_IOS_ADVENTURE_RUNTIME_TESTS=ON` build requires Simulator and
the separate `org.ultimatumproject.tests.package` bundle. Seed verified game data and
legacy saves only there; put a portable fixture at `Documents/world.u4save`.
Launch with `--skip-intro` for native Files/share sheets, cancellation, invalid
backup rejection, replacement confirmation/recovery, and paused resources.
Relaunch with `SIMCTL_CHILD_ZU4_PACKAGE_RELOAD=1` for real-engine imported
adventure loading and saving. `SIMCTL_CHILD_ZU4_PACKAGE_TITLE=1` exercises the
Journey picker; `SIMCTL_CHILD_ZU4_PACKAGE_PORTRAIT=1` selects portrait.
These fixtures are prohibited in physical-device builds.

`SIMCTL_CHILD_ZU4_PAUSE_LAYOUT=1` on the standalone `org.ultimatumproject.tests.layout`
app checks content-sized compact menus with 7–9 actions across two portrait
phone sizes, landscape, and tablet. Every action must be entirely visible,
at least 44 points, with scrolling disabled at ordinary text sizes.
`SIMCTL_CHILD_ZU4_PACKAGE_MENU=1` on the engine fixture exercises the actual
pause menu → Adventure backups → Back → Resume without resource changes.
Add `SIMCTL_CHILD_ZU4_PACKAGE_MENU_HOLD=1` for manual native UI inspection;
enable `ZU4_IOS_DEBUG_TOOLS=ON` to match local device menus with Debug Tools.

The loopback-only `clients/web/tests/serve-adventure-runtime.py --fixture-dir
/explicit/test/dir` runs `adventure-package-runtime-driver.js` with the real
web engine: import native backup, Continue, display Journal favorites/notes,
write a web note, save and export `browser.u4save` back to the test directory.
Set `ZU4_PACKAGE_BROWSER_BACKUP=/explicit/test/dir/browser.u4save` on the
native host runner to validate/install that real browser export.

An opt-in `ZU4_IOS_COMBAT_RUNTIME_TESTS=ON` CMake build runs the actual iOS
combat engine integration suite after loading a fixture adventure. It requires
the separate `org.ultimatumproject.tests.combat` bundle and the Simulator SDK; physical
builds reject the option. Install fixture saves and game data only into that
separate app container, then launch with `--skip-intro`. Console PASS/FAIL output
covers repeat preparation, explicit commitment, Clear/native direction cancel,
range/blockers/identity/weapon checks, special weapon costs, pacing equivalence,
and return to main controls. It holds a repeat preview for a screenshot afterward.
This is automated engine/native-panel testing, not physical finger-touch coverage.

`ZU4_IOS_JOURNAL_RUNTIME_TESTS=ON` similarly requires Simulator and the separate
`org.ultimatumproject.tests.journal` bundle. Launch with `--skip-intro` to exercise the
real journal backend and native views, note editor cancellation, search,
bookmark context, persistence, deletion, and return to gameplay.
The initial run also dispatches the real mode-button targets through 18 repeated
empty/populated clue transitions after the engine event loop starts.
Relaunch with `SIMCTL_CHILD_ZU4_JOURNAL_RELOAD=1` to verify actual engine reload. These fixtures
are not included in ordinary builds or physical-device binaries.

For journal-only standalone UIKit row-fit/target checks, build with
`tests/build-ios-layout-test.sh`, install its `org.ultimatumproject.tests.layout` app on
Simulator, and launch with `SIMCTL_CHILD_ZU4_JOURNAL_LAYOUT=1`. This exercises
the three native list modes and transcript action geometry without loading any
engine adventure. Check the console for PASS output.

For soundtrack/mixer tests, install host SDL2 and run
`bash tests/run-audio-tests.sh`.
It uses a disposable copy of the soundtrack supplied with xu4 and actual OGG
decoding, including mute, track-context and empty-package behavior.
`ZU4_IOS_AUDIO_RUNTIME_TESTS=ON` requires Simulator and the separate
`org.ultimatumproject.tests.audio` bundle; seed its verified archive and fixture saves,
then launch with `--skip-intro`. Relaunch with `SIMCTL_CHILD_ZU4_AUDIO_RELOAD=1`
for persistence checks. Standalone `SIMCTL_CHILD_ZU4_AUDIO_LAYOUT=1` runs real
UIKit action-page geometry at SE portrait and notched landscape dimensions.
