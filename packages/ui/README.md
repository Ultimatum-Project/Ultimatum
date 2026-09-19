# Ultimatum shared UI

This package owns host-level overlay lifecycle without owning game content. A
port declares each overlay's presentation, modality, world-context, gameplay,
dismissal and host policies. The web host provides mutual exclusion for modal
surfaces, replacement/return stacks, initial focus, trigger-focus restoration,
bounded lifecycle events and a non-modal drawer adapter.

Port code still decides whether an action is legal and performs engine-specific
pause/resume work. In particular, the Ultima IV journal retains its engine
controller and the save manager retains its validation and checkpoint rules.
The package does not change save data, `.u4save` v1, settings or engine input.
