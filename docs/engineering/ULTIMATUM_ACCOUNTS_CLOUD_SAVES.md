# Ultimatum Accounts, sync, and account storage

Updated September 17, 2026. The implementation and database changes are in
`main`. The Supabase environments are now separate:

- **ultimatum-test** (`ULTIMATUM_SUPABASE_TEST_URL`) serves QA and local verification.
- **ultimatum-prod** (`ULTIMATUM_SUPABASE_PUBLIC_URL`) is the production account
  environment.

All ten checked-in migrations and the rollback-only save, account-library,
analytics, and feedback suites were applied successfully to `ultimatum-prod` on
September 17, 2026. The production web client now uses `ultimatum-prod`, while
the QA web client uses `ultimatum-test`; their build and deployment preflights
reject a cross-environment package.
Production Auth is enabled with the tested six-digit email OTP length and a
one-hour expiry. Custom Resend SMTP is enabled and the Magic Link/OTP template
matches the tested code-bearing template in `ultimatum-test`. Supabase Site URLs
are `https://ultimatumproject.com/` for production and
`https://test.ultimatumproject.com/` for QA.

## Player experience

The Account hub is available from Saved Games on the web and the iOS pause menu.
The web's player-facing storage surfaces say **saved game** and **cloud save**;
`adventure` remains the internal resource kind and `.u4save` compatibility
format.
It has four sections:

- **Overview** shows each saved game and a plain-language sync state.
- **Games & data** groups account data by game and leaves room for private game
  data packages.
- **Storage** explains the shared 100 MB allowance, shows current saved games and
  recovery history, and lets the player delete old versions or a cloud copy.
- **Account** shows the email and device, links to
  `feedback@ultimatumproject.com`, and provides device-local sign-out.

Email sign-in is passwordless. The user enters the code delivered to their
email; web sessions stay with that browser, while iOS stores its refreshable
session in the device Keychain. Signing out leaves local saved games in place and
does not sign out other devices.

The checked-in production-ready body is `supabase/templates/email-code.html`.
Supabase's **Magic Link** template must include `{{ .Token }}` for this typed-code
flow; the default template only supplies a confirmation link. Custom SMTP needs
a verified sender email and name plus the provider host, port, username, and
password. Keep those credentials in Supabase and out of this repository and all
client bundles.

Sync happens after safe checkpoints and whenever the account refreshes. A local
adventure is uploaded automatically. A linked, inactive local adventure receives
a newer cloud checkpoint automatically. The app never replaces the adventure
currently being played; it asks the player to return to the title screen first.
If this device and the cloud both changed from their shared checkpoint, Overview
shows **Needs your attention** and offers **Use this device** or **Use cloud
copy**. The unselected checkpoint remains in recovery history. There is no
binary merge and no timestamp-only winner.

On iOS, the account UI is a full-screen, accessible web view with 44-point or
larger controls. A hidden short-lived account view performs checkpoint sync only
when a Keychain session already exists. It closes after completion, after an
error, or after a 30-second timeout. Game input and world timers stay paused
while the visible Account hub is open.

## Data integrity

The version-1 `.u4save` package transports every allowlisted adventure file,
including notebook notes and favorites, topics, exploration maps, pins,
discoveries, dungeon exploration, and web conversations. Preferences,
credentials, active-slot selection, and the base game installation are excluded.

Downloads pass SHA-256 transport verification plus the existing package size,
CRC, allowlist, core-save, and metadata validation before installation. Native
installation uses immutable CURRENT/PREVIOUS publication. Web installation uses
an IndexedDB compare-and-swap transaction. A stale destination refuses the
replacement. Uploading never changes local recovery files.

## Account library and quota

`account_resources` is the stable catalog. Each row represents an adventure or
another future account-owned item. `account_resource_versions` contains immutable
versions and device metadata; the resource points at its current version. This
separates an adventure's identity from its revision history. Validated game-data
packages use the same library with `kind = 'game_data'`.

The server enforces a combined **100 MiB (104,857,600 bytes)** per account across
all retained versions. Publication runs under an account advisory lock, computes
the proposed byte total on the server, and refuses writes that exceed the quota.
The storage summary reports current saved-game bytes, historical save bytes,
game-data bytes, item counts, and remaining space. Deleting an earlier
version cannot delete the current version. Removing a resource requires the
current version the client reviewed, preventing deletion after an unseen update.

Existing `cloud_save_heads` and `cloud_save_revisions` are migrated into the
library with their revision identifiers preserved. The old records remain during
this compatibility phase. Existing local saves without a trusted link are not
silently paired to a migrated cloud adventure when both could contain progress;
the Account hub asks the player which copy to keep.

## Access control and publication

Clients can select only rows owned by `auth.uid()` under RLS. Anonymous users and
other accounts cannot read them. Direct insert, update, and delete privileges are
revoked. Authenticated RPCs derive the owner from the session, reject anonymous
identities, validate the portable package and device fields, serialize changes,
and use compare-and-swap against the reviewed current version. A stale writer
gets a conflict and cannot overwrite newer progress. Identical retries are
idempotent.

The browser uses the public Supabase publishable key. No service-role key or SMTP
credential is present in the site or app. iOS only exposes its native bridge to
the packaged local account files and blocks remote navigation.

## Verification and rollout

The account library, analytics, feedback, and private game-data migrations have
been applied to both Supabase projects. As last verified on September 17, 2026,
both projects are healthy and contain the expected tables and RPCs; production
contains no player resources or events. Rollback-only SQL tests cover quota
enforcement, idempotent publication,
immutable history, stale conflicts, owner isolation, direct-write denial,
history deletion, and complete resource deletion. The migrated test adventure
retains its original revision and package.

Web unit and syntax checks cover the passwordless requests, session guards,
publication contract, download integrity, summaries, deletion calls, and local
cloud-link persistence. Site packaging checks ensure the authored account files
and their source snapshot match. Native package/layout and simulator Release
builds verify the bridge compiles with the real iOS target.

Deployment status and exact Cloudflare versions are maintained in
[`clients/site/CLOUDFLARE_SETUP.md`](../../clients/site/CLOUDFLARE_SETUP.md).

### Current storage implementation

Despite the player-facing term “private storage,” packages currently live in
private, owner-scoped Postgres rows—not a Supabase Storage bucket.
`account_resource_versions.package` contains the validated JSON/base64 package,
has a 16 MiB stored-package ceiling per version, and records both logical and
stored byte counts. The 100 MiB quota sums `stored_bytes` for every retained
adventure and game-data version. Moving large packages to private object storage
is a planned scale migration; it must preserve resource IDs, version lineage,
checksums, RLS ownership, conflict semantics, and quota accounting.

### Operational security note

Direct schema inspection reports RLS disabled on
`ultimatum_private.admin_emails`. The table is in the unexposed private schema
and the migration revokes all privileges from `PUBLIC`, `anon`, and
`authenticated`; access occurs inside an authorization-checking
`SECURITY DEFINER` function. Do not enable RLS ad hoc without verifying that
admin-function path. A fresh migration grants no administrator by default. Set
`ULTIMATUM_ADMIN_EMAIL` only in the ignored root `.env.local`, run
`node supabase/scripts/render-admin-bootstrap.mjs`, review the resulting SQL,
and execute it once in the intended project's SQL editor. The renderer never
connects to the database and the email is never bundled into a client.

The September 17, 2026 security advisor did not flag this
table; it only warned that leaked-password protection is disabled, which is not
used by the current email-OTP-only login. The performance advisor reported two
unused resource-version indexes, expected while traffic is low. Recheck these
after meaningful production usage and handle any policy change through a
reviewed migration.

References:

- <https://supabase.com/docs/guides/auth/auth-email-passwordless>
- <https://supabase.com/docs/guides/auth/auth-email-templates>
- <https://supabase.com/docs/guides/database/postgres/row-level-security>
