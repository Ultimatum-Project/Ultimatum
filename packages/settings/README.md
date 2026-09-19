# Ultimatum typed settings

This package defines the platform-owned metadata for typed settings. A port
declares stable keys, value types, defaults, ownership, scope, portability,
apply policy, migration version, and any experience-profile defaults.

The schema describes settings; it does not move their physical persistence.
Ultima IV continues to read and write `xu4rc`. Its experience schema remains
version 3, and existing values remain authoritative. Device/host settings are
explicitly non-portable and are never included in `.u4save` packages.

`settings-registry.js` is the host-side runtime projection. It validates a
manifest, reads effective values from a semantic engine snapshot, and reports
unavailable values without guessing. Writes remain with the current engine UI
until a later common settings surface can preserve the same apply and rollback
semantics.
