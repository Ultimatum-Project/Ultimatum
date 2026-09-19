# Ultimatum session orchestrator

This package owns the platform lifecycle around a versioned `EngineSession`.
It reports structured state changes, acquires an optional single-writer lease,
orders preflight/mount/load/start, gates semantic dispatch, and coordinates
pause, checkpoint, flush, resume, and shutdown without knowing any game rules.

The browser host uses `WebSessionLease`, backed by the Web Locks API when it is
available. Browsers without Web Locks retain the previous single-tab behavior;
the lease reports that it is unsupported rather than inventing a persistent
lock in user storage.

On a non-BFCache navigation the browser host releases its lease synchronously
from `pagehide` while the queued orderly shutdown continues checkpoint and
flush work. A BFCache transition retains the lease and resumes the same session.

`session-progress.schema.json` freezes the additive version-1 progress event.
Game-specific save publication remains outside this package and is supplied as
the checkpoint callback. The xu4 main-thread Asyncify constraint remains inside
its `EngineSession`, so the orchestrator never calls the C++ bridge directly.
