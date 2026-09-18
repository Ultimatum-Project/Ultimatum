# Ultimatum Account Hub and Storage Library

## Purpose

The Ultimatum Account hub is the player's home for cross-device saved games,
account identity, device sessions, support, and the private storage included
with the account. The first release centers on Ultima IV saves. Its
information architecture must also accommodate additional supported games,
game-data packages, and other well-defined Ultimatum data types without turning
the hub into a generic file browser.

An account has a combined private allowance displayed as **100 MB**. The service
stores the canonical quota in bytes and the client only displays values returned
by the service. Saved games and game data share the allowance, but remain
separate resource types with independent validation, retention, and deletion
rules.

Local play remains available without an account. Signing out or removing a
cloud item never silently deletes a local saved game.

Player-facing UI uses **Saved Games**, **Saved Game**, and **Cloud Save**. The
existing `adventure` resource kind, slot fields, and `.u4save` format names are
compatibility identifiers and do not appear as the normal storage metaphor.

## Player questions the hub must answer

The first screen should make these answers obvious:

1. Am I signed in?
2. Are my saved games safe and up to date?
3. Where did I last play each saved game?
4. How much private storage am I using?
5. What is using that storage, and how can I free space safely?

Implementation terms such as buckets, objects, rows, local slots, cloud slots,
revision identifiers, and upload/download direction do not appear in the normal
flow.

## Information architecture

### Overview

The default account screen contains:

- verified email and cloud-save state;
- a compact storage summary, for example **12.4 MB of 100 MB used**;
- saved-game cards ordered by most recently played;
- any sync conflict or storage action that needs attention;
- links to Games & data, Storage, Devices, Support, and Account settings.

The storage summary opens the Storage view. It does not dominate the screen
while the account has ample free space. At 80% usage it becomes more prominent;
at 95% it explains what will and will not sync before the next attempted write.

### Saved Games

Each saved-game card shows the player-facing name, game, progress summary,
location when available, last-played time and device, and one of these states:

- Saved to cloud
- Syncing
- Offline — saved on this device
- Updated on another device
- Needs your attention

Continue is the primary action. Backup history, explicit replacement, export,
and deletion are progressively disclosed under Manage Save.

A saved game has a stable account-wide identifier. A device's local slot is a
placement detail and is not the saved game's identity.

### Games & data

This view groups account content by supported game. An Ultima IV card can show:

- number and combined size of cloud saves;
- installed private game-data package and version, when supported;
- optional related data types introduced later;
- last update and compatible app versions;
- actions to inspect, replace, export, or remove each supported item.

Game-data packages are distinct from saved games. Replacing or deleting game
data must not delete saves, and deleting a saved game must not affect game data.
Compatibility is checked before a game-data package can be selected for play.

### Storage

The Storage view explains the full allowance using player concepts:

| Category | Example contents | Management actions |
| --- | --- | --- |
| Saved games | Current saves for supported games | Open, export, remove from cloud |
| Save history | Earlier recoverable checkpoints | Review, restore, delete older versions |
| Game data | User-provided supported game packages | Inspect, replace, export, remove |
| Other data | Future explicitly supported account data | Type-specific actions |

It shows both category totals and individual items, sorted by size by default.
Every item identifies the game, data type, size, last update, and whether it is
the current version or recoverable history. A **Free up space** flow recommends
old non-current revisions first and states exactly how much space an action will
recover.

The normal interface never exposes arbitrary server paths or allows arbitrary
file uploads. Each resource type has a validator and player-facing management
flow.

### Devices, support, and account settings

Devices lists recent signed-in devices, their last activity, and device-local
sign-out or revocation actions. Support provides a pre-addressed feedback path
to `feedback@ultimatumproject.com` and can include app version and device details
with the player's review. Account settings contains email, cloud-save preference,
data export, and account deletion.

## Storage model

The account library should use stable resources rather than three permanent
cloud slot numbers:

- `account_resources`: owner, type, game, display metadata, current version,
  logical size, stored size, and timestamps;
- `resource_versions`: immutable versions with parent version, checksum,
  storage object reference, creator device, and created time;
- `device_installations`: the last version each device observed and its local
  placement, used for conflict detection;
- `account_storage_usage`: an authoritative server-computed summary by category
  and for the whole account.

Large packages should live as private objects; relational rows hold identity,
metadata, ownership, version lineage, and checksums. Clients must not be able to
claim a smaller size or change their own quota. Publication performs validation,
quota reservation, immutable version creation, and current-version replacement
as one server-authorized operation.

The current cloud-save head and immutable-revision design can migrate into this
model. Existing save slots become adventure resources, and existing revisions
become resource versions. The current compare-and-swap rule remains the conflict
guard.

## Quota behavior

- The displayed total includes all retained private objects that consume the
  player's allowance, including recoverable save history.
- Duplicate physical objects may be deduplicated internally, but quota behavior
  must remain stable and understandable to the player.
- A write that would exceed the allowance is rejected before changing the
  current version. Local progress remains safe and queued for a later retry.
- The error names the required space and opens Free up space.
- Current saved games are never pruned automatically.
- Any automatic history-retention rule must be explicit and deterministic. The
  recommended default is a small protected recovery window per saved game, with
  older non-current revisions offered first for cleanup.
- Deletion states whether it removes a current cloud item, old history, or both.
  It also states that local copies on devices are unaffected.

## Responsive presentation

On iPhone this is a full-screen, vertically scrolling hub with large cards and
temporary detail screens. Overview remains the initial screen; storage and
account administration are progressively disclosed because they are occasional
tasks. On wider web layouts, the same sections may use a sidebar and two-column
content while preserving the same labels and actions.

All interactive controls target at least 44 by 44 points. Storage state is
communicated with text as well as color. Destructive cloud deletion requires a
specific confirmation; routine sync does not.

## Delivery sequence

1. Replace the manual cloud-slot presentation with the Account overview and
   saved-game cards. Keep current explicit transfers under Backup history.
2. Add stable adventure identifiers, automatic sync, device metadata, and the
   two-version conflict choice.
3. Add the authoritative usage summary and Storage view while the only category
   is saved games and their history.
4. Move adventure package bytes to private object storage and migrate existing
   revisions without changing their lineage or checksums.
5. Add validated private game-data resources and show them under Games & data
   and Storage.
6. Add device revocation, account export/deletion, and future resource types as
   separate capabilities.

This sequence makes the first account hub useful immediately while establishing
the vocabulary and resource boundaries required for the complete 100 MB library.
