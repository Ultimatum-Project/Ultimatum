# Ultima IV port descriptor

`catalog-entry.json` is the platform-owned presentation and acquisition record.
`port.manifest.json` remains the engine adapter descriptor. Keeping these
separate allows a future catalog or launcher to change presentation without
moving engine-specific validation into the platform shell.

This directory is the first compatibility boundary for the existing xu4-based
port. The implementation remains in its proven locations while Phase 0.5
extracts contracts around it.

`port.manifest.json` is platform metadata. It does not contain original game
data, user saves, environment identifiers, or credentials.

`save-store.manifest.json` binds the web IndexedDB and native immutable-
generation implementations to the shared SaveStore v1 semantics without
changing either physical layout.

`engine-session.manifest.json` declares the active web semantic-session and
native lifecycle-session providers, their serialized execution models, and the
source files that wire them into each client.

`settings.manifest.json` describes the existing experience, interaction,
touch-device, combat-presentation, graphics, and audio preferences with typed
values and explicit scope. It does not replace or migrate `xu4rc`.

`controls.manifest.json` describes semantic player actions and the verified
desktop and touch profiles. It intentionally declares no controller profile
until controller support has real runtime evidence. Control selections and
device preferences are not portable adventure data.

`diagnostics.manifest.json` declares the stable xu4 session event codes that a
host may record. The shared diagnostics package controls redaction and excludes
game data, saves, typed text, input streams, screenshots and local paths from
support snapshots.

`overlays.manifest.json` separates platform-owned dialogs and drawers from the
port-owned Journal and exploration map, with explicit modality, world-context,
gameplay and dismissal policies. The web overlay host manages presentation and
focus; xu4 still owns journal pause/resume and game-specific legality.
