# Ultimatum semantic session

This package freezes the additive version-1 snapshot envelope already emitted
by the xu4 web bridge. The active browser client uses a compatibility
`EngineSession` wrapper while legacy engine-specific methods remain available.
The active iOS/SDL path uses a C++ `NativeEngineSession` compatibility state
machine for ordered pause, checkpoint, quiesce, resume, input blocking, and
shutdown transitions.

`engine-session-provider.schema.json` defines the shared provider declaration.
The Ultima IV manifest intentionally distinguishes the browser's full semantic
surface from the native lifecycle-compatibility surface; it does not claim that
the native host exposes browser snapshot or intent APIs that it does not use.

The platform-level lifecycle around this contract is implemented separately in
`packages/session-orchestrator`. Keeping the layers separate lets an adapter
serialize engine-specific intents while the platform coordinates launch,
lifecycle, leases, flushes, diagnostics, and host resource release.

Later extraction can replace the wrapper without changing the snapshot schema
or allowing the shell to re-enter an Asyncify-yielding engine directly.
