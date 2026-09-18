# Ultimatum SaveStore contract

`SaveStore` is the platform-owned persistence boundary for validated save
generations. Version 1 exposes `list`, `getGeneration`, `publish`, `restore`,
`quarantine`, and `remove` semantics while allowing hosts to retain their
proven physical layouts.

The web provider projects its existing three IndexedDB slot records into
stable record and generation IDs. The native provider projects immutable
generation directories selected by `CURRENT` and `PREVIOUS`. Neither provider
migrates or rewrites existing data during this compatibility phase.

The strings `ultimatum-adventures-v1` and `ultimatum-adventure` remain frozen
only as legacy database and `.u4save` v1 wire identifiers.
