# Ultimatum storage contracts

Phase 0.5 names logical storage roles while retaining the current physical
locations. The active browser compatibility provider is
`clients/web/dist/library-store.js`.

| Logical role | Existing physical location |
| --- | --- |
| `source-data` | IndexedDB `ultimatum-local-library-v1` / `assets` / `game`; runtime mount `/ultima4` |
| `optional-overlay` | IndexedDB `ultimatum-local-library-v1` / `assets` / `vga` |
| `profile-data` | Existing save store plus the IDBFS working mirror at `/home/web_user/.xu4` |

The save provider retains the physical IndexedDB name
`ultimatum-adventures-v1` solely for existing-data compatibility.

These mappings are compatibility declarations, not a migration. Existing
records, saves, and `.u4save` version 1 packages remain untouched.
