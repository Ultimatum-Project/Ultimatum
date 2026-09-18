# Public release runtime verification

The local release evidence below was collected on 2026-09-15. Deployment has
since completed: as of September 17, 2026 production is Worker version
`cd2bf8ee-1f78-4eb2-8114-adf871bf78a7`, and the public QA site at
`https://test.ultimatumproject.com/` is version
`eda5ee84-503f-4a6a-8000-e0e5134503e9`. Exact hosted checks and rollout state
are maintained in `clients/site/CLOUDFLARE_SETUP.md`.

## Real public engine

The isolated test server runs the actual release engine with bundled DOS data,
development party, debug tools, fixture hooks and diagnostics disabled. ZIPs
are generated in memory from ignored development data and served only on a
test-owned loopback origin; they are not part of the public package.

The final 393 × 700 portrait-frame run passed 33 checks:

- Actual chunked/content-addressed engine boot without game data or saves.
- URL-import rejection of missing files, wrong executable version, unsafe
  paths, duplicate filenames, corrupt CRC, oversized files and excessive entries.
- Valid import publishes exactly 103 reviewed files; unrelated files and
  embedded saves are excluded.
- Rejected replacements preserve active bytes and the durable library.
- Injected IndexedDB commit failure rolls active files back; injected staging
  write failure leaves active files untouched. These are fault injections at
  the real persistence/filesystem boundaries, not mocked engine runs.
- Arbitrary VGA ZIP rejection and verified VGA acceptance through file input.
- Original story/virtue character creation, responsive Wait controls, no debug
  capability, durable manual save, reload and Continue with matching Avatar/turns.

The preceding 844 × 390 landscape-frame and desktop runs passed all 32 checks
then present; the additional staging-write-failure check was added afterward
and passed in the final portrait run. Read-only frame geometry confirmed the
393-pixel homepage had no horizontal overflow and its 52-pixel Play button was
visible within the initial 700-pixel viewport. In-app-browser viewport overrides
did not resize its native viewport, so real-sized iframes were used instead.

## Existing onboarding/save regression

All 17 checks in the existing real-engine onboarding/save suite passed on a
fresh test origin: legacy migration, cancellation, original creation, automatic
and manual checkpoints, modal locking, rename, export/import, native metadata,
corruption rejection, aborted transactions, stale-tab protection, recovery,
confirmed inactive-slot deletion, emergency export and reload/Continue.

An initial run on an old QA origin failed its first-run migration precondition
because previous QA state remained. No user data was cleared or migration
logic weakened; the full suite was rerun on a fresh isolated origin.

## Static checks and limits

25 web tests and 3 public-package tests pass, along with JS syntax checks and
git whitespace checks. Package tests inspect preload boundaries, chunk size/
checksums/reassembly, real client/source routes and absence of fixture payloads.

Physical iPhone/Android browsers and real browser quota exhaustion remain
unverified. Hosted HTTPS, routing, core headers, account-hub reachability and
real 404 behavior have since been checked as recorded in the deployment guide.
Cloud quota enforcement, storage deduplication and cross-device sync are
separate backend/runtime concerns and are not established by this local suite.
An injected failure is recovery evidence, not a claim of testing every browser
quota failure. This initial compatibility profile deliberately rejects other
releases, modified/translated files and repacked VGA archives.
