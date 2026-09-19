# Ultimatum catalog contract

This package defines the versioned, platform-owned catalog record used to
present a game independently of its engine adapter. A catalog entry identifies
one game and port, supplies localizable presentation and acquisition guidance,
and points to the port descriptor that owns engine-specific capabilities.

`ports/ultima-iv/catalog-entry.json` is the first catalog entry. The validator
can cross-check it against its port descriptor so catalog and adapter identity,
edition IDs, and compatibility ranges cannot silently drift apart.

Artwork is deliberately optional. Any future artwork record must carry a local
asset path and a provenance record before it can appear in a release.
