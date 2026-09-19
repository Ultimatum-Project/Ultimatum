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

Phase 1's local-library model is defined separately in `packages/library`.
The browser provider projects the legacy `game` record into that model through
`inspect()` while returning the same stored source data to the active client.
The projection performs no writes and does not opt an installation into OPFS.
New imports use `publishInstall()` to commit `game` and
`install-ultima4-default` together inside the existing `assets` object store.
No database-version upgrade or bulk migration is performed.

## Phase 1.4 providers

`src/storage-provider-contract.js` defines the common provider shape used by
the browser host: a stable ID and capabilities plus `beginTransaction`,
`read`, `stat`, `list`, and `estimate`. The active IndexedDB compatibility
store implements that shape without changing its database, object store, or
record keys.

`src/opfs-storage-provider.js` adds an experimental byte-oriented OPFS
implementation under the isolated `ultimatum-experimental-v1` directory. It
validates logical references, prevents traversal, scopes transactions,
serializes commits, restores earlier bytes when an in-process publication
fails, lists namespaced files, and reports browser quota estimates.

OPFS is registered for capability testing only. IndexedDB remains the selected
provider, and there is deliberately no automatic copy, migration, deletion,
or user-facing backend switch. Because current browser OPFS APIs do not supply
a general multi-file atomic commit, the provider declares
`atomicTransactions: false`; it must not hold authoritative irreplaceable data
until crash recovery and migration verification are implemented.
