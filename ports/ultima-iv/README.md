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
