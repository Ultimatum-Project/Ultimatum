# Web onboarding and save management runtime results

Originally verified September 15, 2026 with the real Release-mode WASM engine, real
IndexedDB and the shipped UI handlers. Fixture engines/servers and test-owned
slots were isolated on loopback origins; the user's LAN storage was untouched.
Normal bundled-data Release build and the separate fixture build succeeded.
`npm run check` and all 17 unit/contract tests passed.

Reverified September 17, 2026 on Windows using the exact Archive.org English
DOS/EGA archive supplied by the user. Its SHA-256 is
`6BBED280643FC40AEB99AEB0B8215476B6F5E9AEB1170974FF607DAFB6534C98`.
The archive and extracted data remained under the ignored `.cache/test-data`
tree and were neither committed nor deployed. All 43 current contract tests
and all 17 desktop onboarding/save lifecycle checks passed against the real
WASM engine. The loopback server was also corrected to read and emit UTF-8
explicitly on Windows.

The cold-start Account regression also verifies that a manually opened sign-in
dialog remains active while the engine filesystem initializes. If game data is
still required, its importer does not displace Account and becomes active only
after the player closes Account; both Done and Escape use the same handoff. The
required importer also includes its own Account button, so sign-in never depends
on clicking through the title screen before engine startup finishes.

## Browser results

| Suite | Viewport | Result |
|---|---|---|
| Full onboarding/save lifecycle | 393 × 700 frame | 17 passed |
| Full onboarding/save lifecycle | Desktop | 17 passed |
| Full onboarding/save lifecycle | 844 × 390 landscape frame | 17 passed |
| Existing Menu/Experience/Debug regression | 393 × 700 frame | 53 passed, including preference reload |
| Existing special conversations | 393 × 700 frame | 23 passed |
| Existing tap routes | 393 × 700 frame | 8 passed |
| Existing HUD/Wait/Search geometry | 393 × 700 frame | 8 passed |

The lifecycle tests verify:

1. Title waits for a choice and preserves the existing browser adventure.
2. Cancel creation returns to title without publishing or damaging other slots.
3. Actual name/character selection, all 24 story pages and seven virtue choices
   create a new Avatar through the original engine calculation.
4. Ten safe turns publish a durable checkpoint and retain its predecessor.
5. Manual Save journey publishes actual party progress.
6. Adventures & saves owns a paused engine and rejects gameplay underneath.
7. Rename changes only the chosen slot label.
8. UI export/import round-trips actual save bytes into another slot.
9. Real engine saves retain the active adventure's loaded native-format
   journal, explored maps, pins and discoveries.
10. Classic multi-file import preserves all those metadata bytes.
11. Bad package checksums reject import without replacing good checkpoints.
12. An actual aborted IndexedDB transaction leaves the published slot intact.
13. A second store writer causes stale-checkpoint publication to be rejected.
14. Shared native validation rejects corrupt metadata; confirmed recovery
    restores a validated predecessor.
15. Delete can be cancelled and confirmed deletion affects only its slot.
16. An injected QuotaExceededError at the storage boundary preserves the
    durable slot, offers a valid emergency export and permits a safe reload
    after export. This is deliberate fault injection, not a claim that real
    browser/device quotas were exhausted.
17. Reload returns to title; Continue restores the selected Avatar, moves,
    renamed slot and optional metadata, without changing the other adventure.

Save/re-save may legitimately change the creature table even at the same move
count. Metadata-preservation assertions compare those files byte-for-byte,
while backup/import assertions compare the complete published checkpoint.

Runtime testing found and fixed the missing SDL checkpoint-event dispatch,
missing legacy-journal handling, trusted-tap file-picker activation, and
emergency-export/reload behavior. Test-driver waits were corrected to await
rendered/enabled controls rather than racing asynchronous slot updates.
Visual review simplified the title to Continue / New Game / Manage; backup
controls expand on demand rather than crowding the first screen. Full paused
management remains available from Menu. These surfaces do not resize the HUD.

## Reproduce

```sh
cd clients/web
npm run check
npm run test
npm run build:engine
npm run test:runtime:build
npm run test:runtime:serve
```

Open `/responsive-runtime.html?suite=onboarding` on the loopback test server.
For desktop use `/?onboarding_suite=1`. Run suites sequentially or on separate
origins: their mutable fixture adventures must not share a live store.
`#runtimeReport` reports assertions across engine reloads. The onboarding
driver resets only test-owned Slots 2/3 and must never run on user/production
storage. `suite=review` displays the real title without an automated driver;
add `layout=landscape` for an 844 × 390 frame.

## Boundaries and remaining proof

Slot records are authoritative; IDBFS is the legacy working-copy mirror.
Each slot atomically publishes a current checkpoint plus its immediate
predecessor. Binary/metadata validators and CRC32 checks run before activation.
CRC32 detects accidental corruption; it is not authentication or a security
signature. Invalid/partial legacy saves are quarantined for export rather than
silently replaced or loaded. Public builds remain bring-your-own-game-data.

Physical iPhone Safari, Android Chromium, actual quota exhaustion and abrupt
OS suspension remain unverified. Background checkpoint requests are best
effort, so the UI instructs players to Save before leaving. Cross-client
iOS → web → iOS package adapters/round trips and authenticated cloud sync are
not implemented. Imported native metadata is retained, not yet updated by
web journal/exploration/pin gameplay surfaces. Native iOS behavior is unchanged
by the WEB-gated intro and SDL adapter additions.
