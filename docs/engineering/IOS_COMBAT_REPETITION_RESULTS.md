# iOS combat repetition reduction

## Delivered behavior

- Repeat Attack lives in the previously empty D-pad center. It prepares, rather
  than commits, the active fighter's last enemy. Attack names that enemy and
  commits the original attack; Clear cancels without spending a turn.
- Memory is per fighter and per battle, and uses a unique creature-instance
  identity rather than a coordinate or allocator address. Clones receive new
  identities. Dead/replaced enemies, unreachable paths, disabled fighters, and
  changed weapons cannot repeat. Current coordinates, range, and blockers are
  recomputed before preparation and confirmation. A stale prepared repeat never
  becomes a blind directional attack.
- Original weapon attack paths, hit/damage calculations, consumable deductions,
  returning weapons, tile effects, and finishTurn remain authoritative. Repeat
  never casts a spell or automates a companion.
- Menu → Controls → Combat pacing offers Standard (default) and Fast. Controls
  is also available in the battle pause menu. Fast halves longer flashes (with a
  33ms floor), shortens the existing 50ms round pause to 25ms, and omits the
  repeated fighter/weapon turn-start banner already represented by the HUD.
  Outcome, condition, death, failure, and resource messages remain unchanged.
- Pacing persists atomically in xu4rc across adventures independently of profiles.
  Invalid/missing values default to Standard; failed setting writes roll back.

## Design standard

Read the canonical PC → iOS Interface Playbook before implementation. The new
frequent action uses existing thumb-reachable space without shrinking controls
or moving the world. Selection and commitment remain separate with explicit
target identity and cancellation. No playbook deviation is required.

## Verification

- All 13 portable mobile suites pass, including identity/copy/assignment safety,
  weapon/per-fighter memory, delay boundaries, preference round-trip/defaults,
  and preservation through profile changes.
- Production arm64 Simulator Release and signed iPhone Release builds succeed.
- The opt-in Simulator-only integration fixture runs the real engine inside the
  separate generic combat-test app. User adventures are never selected or
  modified by this fixture. Physical-device builds reject this test option.
- Runtime verifies first-attack availability, per-fighter memory, no-turn repeat
  preparation, Clear cancellation, native direction-prompt cancellation, and
  one-fighter turn/resource accounting through the production dispatcher.
- Runtime verifies out-of-range, blocked, moved, disabled, weapon-changed,
  stale-preview, and dead-enemy-replacement cases, plus Flaming Oil, Halberd,
  and Magic Axe paths/costs. Exiting battle returns to main controls and a new
  battle starts without remembered attacks.
- Seeded Standard/Fast attacks produce identical damage (43). The measured
  final presentation times were about 0.154s and 0.099s respectively; timing is
  illustrative rather than a performance guarantee.
- Inspected the real-engine portrait screenshot at
  /private/tmp/u4-combat-repeat-final.png: Repeat Attack fits the D-pad center,
  Attack names Rat, Clear remains visible, and the target/path highlight and
  ordinary controls remain available without moving or covering the viewport.
- Final engine evidence is in the Simulator device's
  data/tmp/u4-combat-runtime-final-err.log; build logs are
  /private/tmp/u4-combat-sim-final.log and /private/tmp/u4-build12-final.log.

## Remaining coverage

The Mac is locked, preventing computer-use finger/keyboard interaction. Runtime
tests invoke production engine actions and the native direction-cancel API;
they do not establish physical touch, rotation, or interactive pacing-picker
coverage. Full battle/endgame completion is not claimed. Isolated screenshot
inspection supplements, but does not replace, those checks.

## iPhone delivery

Inspected the installed main bundle (version 1.0 build 11) and built the next
integer, build 12, with build-local-device.sh and the configured development team.
Installed over the configured development bundle on the connected iPhone 14 Pro and
launched it successfully, preserving its app data. A subsequent device app
inventory confirms version 1.0, bundle version 12. No TestFlight upload occurred.
