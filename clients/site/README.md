# Ultimatum Project public site

The public narrative surface lives here; `clients/web` remains the gameplay
client. Authored homepage assets are tracked under `dist`. `build` is an
ignored, generated deployment package containing the homepage, real game at
`/play/`, the optional Supabase-backed Account hub, an administrator dashboard,
and corresponding engine source/credits. There is no public iOS download.

## Build and preview

Copy the repository-root `.env.example` to `.env.local` and supply the public
and QA Supabase URLs, publishable keys, and project labels. The populated file
is ignored by Git; CI may supply the same names as environment variables.
Public and QA packaging fails closed when these values are absent or malformed.

```sh
cd clients/site
npm run build
npm run build:qa
npm run test
npm run dev
```

Open `http://localhost:4180/` or the development machine's LAN address on port 4180.
The gameplay client at this origin starts with a game-data picker, not a
development party. Browser saves from port 4173 do not automatically appear
here: export a `.u4save` backup there and import it into this origin's slots.
No account is needed for local play.

`npm run check` is syntax-only. The public-build tests inspect ignored generated
files under `build/` and the prepared, checksum-verified VGA input under the web
client cache, so run the applicable preparation and `npm run build` before
expecting the complete suite to pass. A fresh checkout deliberately omits those
artifacts.

On Windows PowerShell, direct Node test runs need explicit glob expansion (the
portable `npm test` command remains preferred):

```powershell
$files = Get-ChildItem tests\*.test.mjs | ForEach-Object FullName
node --test $files
```

The public build includes the soundtrack supplied with xu4 and its in-game
credits. Alternate soundtrack packs are not packaged and there is no soundtrack
selection control.

The normal build bootstraps the project-local toolchain pinned by
`clients/web/toolchain/versions.env`; no global Emscripten install is required.
On Windows this uses Git for Windows and the Visual Studio C++ developer
environment automatically. Python 3 is required. The build forces development game
data, diagnostics and fixture hooks OFF, while the current early-access
production policy explicitly compiles Debug Tools in. It uses a separate CMake
directory and engine output under this client's ignored `.cache`.
It does not alter the normal LAN engine build. Preload inspection fails closed
unless engine assets are under `/conf`, `/graphics`, `/sound`, or the xu4
soundtrack directory, plus the exact checksum-verified graphics-only
VGA ZIP. Original DOS files, saves and DOS patch programs
are always excluded. No separate VGA upload is needed after game-data import.
Old decorative game-data sprite URLs are removed from the public CSS.
`build:qa` writes `build-test/` with the same compile-gated Debug Tools but the
separate test cloud environment. Both packages record `qaDebug: true`; the
cloud-project marker keeps production and test deployments isolated.

Cloudflare's 25 MiB asset limit is handled with <=20 MiB content-addressed
engine asset chunks. `Module.getPreloadedPackage` supplies the verified,
reassembled buffer to the unmodified Emscripten preload machinery. Generated
JS and WASM are content-addressed too, avoiding mixed cached engine versions.
The public loader's absolute local `.data` path is normalized before packaging.
Hosted HTTPS verifies chunk SHA-256; the HTTP LAN preview checks chunk sizes.
Splitting handles hosting limits, not download-size optimization.

Matching engine source, build scripts, engine assets, licenses and contributors
are included under `/source/`, with paths/checksums in `/source-files.json` and
a human-readable `/source.html`. Original DOS data and music payloads are not
source snapshot inputs. VGA attribution and terms are preserved under
`/licenses/ultima-iv-vga.txt`. Changes to shared native engine source elsewhere in the dirty
worktree are preserved; this script builds from the current checkout.

## Deployment

`wrangler.jsonc` is prepared for Cloudflare Workers Static Assets with real 404
handling, not a catch-all SPA fallback. `_headers` supplies MIME hardening,
restricted scripting/WASM, private-device feature permissions and cache rules.
External ZIP fetching follows browser mixed-content and CORS rules; use HTTPS
ZIP URLs on the public HTTPS site. The build injects only a Supabase publishable
key. No Supabase secret key, service-role key, or login credential belongs in
the static release or `.env.local`.

The early-access release with compile-gated Debug Tools is deployed to
`https://ultimatumproject.com/`; the QA build is deployed separately to
`https://test.ultimatumproject.com/`. Production and test retain separate
Workers, cloud projects, and browser-storage origins.

For subsequent releases:

1. Build and verify the release locally.
2. Authenticate a current Wrangler CLI to the user's Cloudflare account.
3. Deploy this client using `wrangler.jsonc`; verify the resulting Workers URL.
4. Verify `ultimatumproject.com` over HTTPS, `/play/`, direct game-data
   importing, source access, and 404 responses.

Do not create a second hosting project or publish to an unrelated account.

## Isolated runtime QA

```sh
python3 tests/serve-runtime.py --port 4181
```

`/runtime/index.html` tests actual public WASM, real IndexedDB and actual import
controls, including malformed archives, preservation, VGA, new-game creation,
saving and reload/Continue. Use a fresh test-owned loopback origin for a full
run; do not run on user/production storage. Fixtures are produced in memory
from ignored local data and are never copied into `build` or served by the
normal preview. The server listens only on loopback.

`/runtime/responsive.html` runs in a real 393x700 iframe; `?layout=landscape`
uses 844x390. `?surface=home` displays the homepage at the same exact size.
Only QA `/runtime/` responses permit same-origin framing; production remains
non-embeddable. See `docs/engineering/PUBLIC_SITE_RUNTIME_RESULTS.md`.
