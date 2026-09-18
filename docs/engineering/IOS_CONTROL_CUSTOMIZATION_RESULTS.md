# D-pad sizing and Flip controls — September 14, 2026

Menu → Controls now offers D-pad size (Small, Standard, Large), Flip controls
(On/Off), and a Touch interactions submenu for the existing bump, tap-to-interact,
and tap-to-walk settings. The main menu and size picker each use three choices
plus fixed Back navigation, preserving the short landscape composition.

Small requests 44-point direction buttons, Standard preserves the 52-point
default, and Large requests 64-point buttons. Buttons fit available space and
never drop below 44 points. Tight landscape gutters limit the maximum size;
the same size cap applies on either side so flipping does not resize the D-pad.
The world frame is unchanged by either preference.

Flip controls mirrors the action columns and moves the D-pad to the opposite
side without reversing directional semantics. In landscape, the party rail
follows the D-pad while the status/minimap rail follows the actions, avoiding
overlap. A cramped party rail uses three columns instead of four shorter rows.
Direction prompts are positioned opposite the D-pad and fitted so even Large
cannot cover them. Settings changes stop held movement and relayout the existing
controls without recreating them.

`dpadSize` and `flipControls` persist in the device-level `xu4rc`, outside
adventure snapshots and Experience Profile overrides. Missing/invalid values
default to Standard/Off. Atomic write failure preserves the previous preferences
and reports that controls were unchanged. Profile changes/restoration do not
reset these device preferences.

## Evidence

- All 12 portable mobile suites passed, including setting round trips,
  backward-compatible defaults, invalid values, and preservation across
  Experience Profile changes/restoration.
- Final arm64 Simulator Release and signed device Release builds passed.
- Build 11 installed over the configured development bundle on the paired iPhone 14 Pro
  and launched successfully. Device verifies version 1.0, bundle version 11.
  Existing app data was preserved; no TestFlight upload was performed.
- Native UIKit assertions passed all 48 geometry combinations: eight
  phone/tablet dimensions × three sizes × two sides. Assertions cover safe-area
  containment, 44-point minimum targets, action/D-pad separation, symmetric
  flipping, world preservation, and direction-prompt clearance.
- Real engine verification used a pre-existing test-party fixture in the
  isolated iPhone 17 Pro Simulator app; no real adventure was loaded or changed.
  Fixtures supplied preferences before launching with `--skip-intro`.
  Small/On rendered 44-point buttons on the right and action columns on the left.
  After restart, Large/Off rendered 64-point buttons on the left and action
  columns on the right. Screenshots were visually inspected; the portrait
  world and party layout remained fixed.

The native geometry assertions are part of `ios_party_layout_test.mm` and can
be built using `bash vendor/ultima4-ios/tests/build-ios-layout-test.sh`.

## Remaining runtime checks

The host Mac remains locked, so actual touch selection of the preference rows,
Back/Resume return, movement/hold/release in both layouts, live rotation,
direction selection/cancellation, dungeon/combat controls, and landscape HUD
visual inspection remain unverified. Fixture-based restart and geometry checks
do not establish those interactive gameplay flows as complete. Very large
accessibility text remains a native visual check.
