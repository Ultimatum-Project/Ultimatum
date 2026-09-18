# Ultimatum Project

Ultimatum is a local-first platform for playing lawfully obtained or expressly
authorized classic-game data through modern engines. This repository currently
contains the production Ultima IV implementation for native iOS and the web,
plus the account, cloud-save, public-site, and deployment infrastructure around
it. The multi-game platform interfaces described in the architecture are an
incremental extraction target, not a finished generic SDK.

Public web builds are bring-your-own-data. Original Ultima IV DOS files, user
saves, local build products, and credentials must not be committed or included
in public packages.

## Repository map

- [`vendor/ultima4-ios`](vendor/ultima4-ios/) — xu4-derived C/C++ engine,
  shared gameplay semantics, Objective-C++ iOS host, and native tests.
- [`clients/web`](clients/web/) — Emscripten/WebAssembly engine host,
  responsive gameplay shell, importer, local save store, and cloud client.
- [`clients/site`](clients/site/) — public homepage and the Cloudflare package
  that publishes the game under `/play/` with corresponding source and notices.
- [`packages`](packages/) — versioned platform contracts being extracted around
  proven behavior, including port descriptors, semantic sessions, imports, and
  logical storage mappings.
- [`ports/ultima-iv`](ports/ultima-iv/) — the first schema-validated port
  descriptor; implementation remains in its established locations during the
  Phase 0.5 compatibility migration.
- [`supabase`](supabase/) — migrations and SQL tests for accounts, immutable
  resource history, quota enforcement, administration, and feedback.
- [`docs`](docs/) — architecture, design specifications, implementation status,
  and dated runtime evidence.

## Start with these documents

1. [`docs/architecture/ULTIMATUM_PLATFORM_ARCHITECTURE.md`](docs/architecture/ULTIMATUM_PLATFORM_ARCHITECTURE.md)
   is the canonical current-state, target-contract, and migration specification.
2. [`docs/engineering/ULTIMATUM_ACCOUNTS_CLOUD_SAVES.md`](docs/engineering/ULTIMATUM_ACCOUNTS_CLOUD_SAVES.md)
   describes the implemented account and cloud-storage baseline.
3. [`docs/engineering/WEB_CLIENT_STATUS.md`](docs/engineering/WEB_CLIENT_STATUS.md)
   summarizes the current browser client.
4. [`docs/design/PC_to_iOS_Interface_Playbook.md`](docs/design/PC_to_iOS_Interface_Playbook.md)
   is required reading before changing mobile UI or interaction behavior.

When documents disagree, prefer executable code and tests, then the platform
architecture, current status documents, and finally dated result logs. Several
engineering files intentionally preserve chronological evidence and therefore
contain statements superseded by later sections.

## Development entry points

### Web gameplay client

From `clients/web`:

```sh
npm run check
npm test
npm run dev
```

Building the real engine requires the pinned Emscripten toolchain and local,
ignored Ultima IV data. See [`clients/web/README.md`](clients/web/README.md) for
bootstrap, import, runtime-fixture, and public-build details.

### Public site

From `clients/site`:

```sh
npm run check
npm run build
npm test
npm run dev
```

The build consumes generated web-engine and verified media inputs. Deployment
is a separate, explicit operation; none of the build commands deploys a site.
See [`clients/site/README.md`](clients/site/README.md) and
[`clients/site/CLOUDFLARE_SETUP.md`](clients/site/CLOUDFLARE_SETUP.md).

### Native iOS client

Native builds and runtime suites require macOS, Xcode, CMake, signing for a
physical device, and verified game data for isolated test applications. Start
with [`vendor/ultima4-ios/README.md`](vendor/ultima4-ios/README.md),
[`vendor/ultima4-ios/ios/CONTROLS.md`](vendor/ultima4-ios/ios/CONTROLS.md), and
[`vendor/ultima4-ios/tests/README-mobile.md`](vendor/ultima4-ios/tests/README-mobile.md).

## Test prerequisites and generated files

A fresh checkout deliberately lacks ignored engine builds, public packages,
game data, and verified VGA artifacts. Tests that inspect those products must
run after the corresponding build or preparation step. Some web contract tests
also compile a small C++ harness and therefore require a `c++` compiler on
`PATH`.

The checked-in Node test scripts use shell globs. On Windows PowerShell, expand
the files explicitly:

```powershell
$files = Get-ChildItem tests\*.test.cjs | ForEach-Object FullName
node --test $files
```

For the site, use `*.test.mjs` instead. Syntax-only checks do not require the
generated engine or public package.

## Compatibility and release invariants

- Preserve existing browser installations, native slots, and `.u4save` version
  1 import/export unless an explicitly approved migration says otherwise.
- Publish imports and saves atomically and retain the last validated generation
  after failure.
- Keep game rules and action legality in the engine; clients submit semantic
  intent and render declared capabilities.
- Keep local play and saves usable without an account. Save sync and private
  game-data upload are separate, explicit actions.
- Exercise changed gameplay in the real engine and applicable UI, including
  cancellation and return to main controls. Builds and unit tests alone do not
  establish feature completion.
- Keep production, QA, and local browser origins isolated. Verify both backend
  migration state and the deployed frontend before reporting cloud availability.
