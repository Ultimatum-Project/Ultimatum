# Ultimatum semantic actions and control profiles

Ports describe player intent as semantic actions. Hosts bind those actions to
keyboard, touch, pointer, or controller inputs without moving game legality
into the platform. The live engine snapshot remains authoritative for whether
an action is currently available.

Every port must provide a verified desktop profile. Touch profiles are included
only where the interaction has been designed and tested as a first-class path;
controller profiles are not declared until controller support is real.

Profiles are device/host preferences, not adventure data. The validator rejects
duplicate physical bindings and profiles that omit required actions.
