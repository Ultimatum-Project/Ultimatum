# Ultimatum local-library contract

The local library records what this device can launch. It is intentionally
separate from the catalog and from account/cloud metadata: seeing a catalog or
cloud entry does not imply that copyrighted source data exists locally.

The first browser provider projects the existing
`ultimatum-local-library-v1/assets/game` record into this model without moving
or rewriting it. A missing record becomes `not-installed`; a recognized legacy
record becomes `playable`; and an unrecognized record becomes
`repair-required`. This is a read-only compatibility projection, not a storage
migration.

New imports atomically publish the existing `game` payload and a separate
`install-ultima4-default` record in the same IndexedDB transaction. Older
installations remain readable when that new record is absent; their projected
record is not written back merely by launching the game.
