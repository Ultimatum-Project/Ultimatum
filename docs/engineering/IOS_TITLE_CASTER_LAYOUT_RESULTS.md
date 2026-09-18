# iOS title and party selection layout — September 14, 2026

The full opening artwork now preserves its 320:200 aspect ratio while its
bottom aligns with `zu4MapFrame`. Portrait uses the world width; landscape
fits the artwork inside the safe area above that same baseline. Keyboard entry
retains the previous full-screen rendering path.

Player selection now uses an explicit party-selection panel style, rather than
the generic scrolling choice sheet. Up to eight available members use four
two-column rows, with name/condition and HP on separate lines. Cancel remains
in the fixed header. The casting prompt is `Choose caster`. Eligibility,
active-player shortcuts, and cancellation semantics are unchanged.

This follows the mobile playbook's temporary-sheet and finger-sized-target
guidance. Short screens and larger text can expand to the safe area; extreme
accessibility text can still scroll if it cannot fit, to avoid inaccessible
choices.

## Verification

- All 12 portable mobile test suites passed.
- Final arm64 Simulator Release and signed arm64 device Release builds passed.
- Real engine title-screen screenshot on the isolated iPhone 17 Pro Simulator
  verified the raised artwork and native opening menu. No adventure was loaded.
- Standalone native UIKit assertions passed on iPhone 17 Pro portrait:
  eight 177×48-point buttons, four rows, scrolling disabled, card below the
  world. World baseline 514 points; title baseline 514 points.
- The same assertions passed on iPhone 14 Pro landscape:
  eight 150×48-point buttons, four rows, scrolling disabled. World and title
  baseline 365 points. The existing landscape side-sheet composition is retained.
- Final app installed over the configured development bundle and launched on the
  paired iPhone 14 Pro. Device reports marketing version 1.0, bundle version 10.
  Existing app data was preserved; no TestFlight upload was performed.

Native layout tests can be built with
`bash vendor/ultima4-ios/tests/build-ios-layout-test.sh`. Install the resulting
standalone app on a simulator and launch `org.ultimatumproject.tests.layout`. It asserts
the eight-member layout and shared title geometry, logs `PASS`, and writes
`Documents/party-layout.png`. It does not load game saves or exercise the engine.

## Remaining runtime coverage

The host Mac is locked and computer-use automatic unlock failed. Actual touch
selection, cancellation back to the main controls, rotation while selecting,
and keyboard/name-entry return could not be exercised. Native layout assertions
and successful builds do not establish those gameplay flows as complete.
Extreme accessibility sizes also remain a visual/runtime check.
