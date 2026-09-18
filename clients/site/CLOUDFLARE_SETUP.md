# Cloudflare setup

Deploy to the user's Cloudflare account, not a separate managed hosting account.
The existing `wrangler.jsonc` publishes `build/` as Workers Static Assets:
homepage `/`, game `/play/`, engine source `/source/`, and real 404s.
The `.openai/hosting.json` is a portable static-output declaration; no managed
Sites project has been registered. Do not register a second project.

## Prepared locally

- Official Wrangler pinned to **4.132.0**, requires Node 22+.
- `node scripts/cloudflare-preflight.mjs` is a read-only public release gate. It requires
  only the soundtrack supplied with xu4 and rejects alternate music, private saves, credentials,
  oversized assets, and mismatched engine or corresponding-source checksums.
- Production and QA both compile Debug Tools in during the current early-access
  phase. Preflight requires that explicit setting and still enforces the
  audience-specific cloud project and Worker package.
- No original DOS game data may be bundled. The graphics-only upgrade remains
  subject to the existing checksum and attribution verification.

## Account connection and deployment

1. Sign in to the Cloudflare dashboard with the existing Google login. Verify
   the account and whether `ultimatumproject.com` is already an active zone.
2. Authenticate the pinned CLI with
   `npm exec --yes --package=wrangler@4.132.0 -- wrangler login`.
   The user approves the browser authorization. Do not paste tokens into chat,
   commit credentials, or grant broader access merely for convenience.
3. Run `npm exec --yes --package=wrangler@4.132.0 -- wrangler whoami` to verify
   the intended account. Account connection alone does not deploy anything.
4. When the game release is ready, run the normal `npm run build`, relevant
   tests, and `node scripts/cloudflare-preflight.mjs`.
5. Deploy to `workers.dev` first using the pinned CLI and `wrangler.jsonc`.
   Verify HTTPS, `/play/`, import, save/reload, licenses/source, and 404 handling.
6. Add `ultimatumproject.com` as the Worker's custom domain only after reviewing
   existing DNS. Preserve unrelated mail/domain records. If the domain isn't
   on Cloudflare nameservers, the registrar change requires user coordination.
7. Verify the apex HTTPS site and `/play/`. Plan `www` separately after confirming
   whether it is used; do not replace existing hostnames blindly.

The packaged client includes the Supabase account/provider contract. Local game
data and saves remain available without an account. Signed-in users can opt into
checkpoint sync and separately upload validated private game-data packages;
both consume the same server-enforced 100 MiB allowance.

## Current production deployment

Repository update 2026-09-18: current source packages only the soundtrack
supplied with xu4. The deployment recorded below predates that removal and must
be replaced before it represents the repository's current release policy.

Verified September 17, 2026: the early-access package with compile-gated Debug
Tools was deployed to Worker `ultimatum-project`, version
`0725a961-b34e-4662-bc4e-25668ef7f574`. Both
`https://ultimatumproject.com/` and `/play/` return HTTP 200; the hosted browser
reaches the bring-your-own-data importer and opens the passwordless Account hub.
The cloud client asset returns HTTP 200, an unknown route returns HTTP 404, and
deployed metadata reports `musicIncluded: true`, `qaDebug: true`, and
`cloudProject: ultimatum-prod`. This build uses shell cache version
`20260917-mobile11`; the web client now presents named Saved Games rather than
numbered adventure slots, while retaining the compatible three-slot storage and
`.u4save` format underneath. Required Game Data provides its own Account entry
point, closes before Account opens, and returns only after the player closes
Account. Its account game-data scan
uses supported filesystem metadata calls, and north-up overhead dungeons route
direction input through cardinal movement rather than relative turning.

## Test site: test.ultimatumproject.com

### Current audience — public test site

Repository update 2026-09-18: the deployed QA package described below predates
the alternate-soundtrack removal and must also be replaced.

Verified September 17, 2026: a QA package with Debug Tools was
deployed to Worker `ultimatum-project-test`, version
`47ccdcf7-bc2c-4447-8b54-0e5ae783594c`. The test homepage, `/play/`, and cloud
client return HTTP 200; a real browser opens the passwordless Account hub.
Deployed metadata reports `musicIncluded: true`, `qaDebug: true`, and
`cloudProject: ultimatum-test`. The isolated real-engine suite verifies the
cold-start Account/Game Data handoff and the complete create, save, restore,
export, delete, and import lifecycle using the new Saved Games terminology. It
previously
verified that overhead East moved from `(1,1)` to `(2,1)`
and updated facing to East before the preceding engine version was promoted to
production. This QA release retains that exact content-addressed engine package
and simplifies the authored homepage around the platform promise, the first
supported game, local-first play, and one primary Play Ultima IV action.
This package was rebuilt from source on Windows with the checked-in pinned
Emscripten 6.0.9/CMake/Ninja/SDL2/libxml2 contract and checksum-pinned,
deterministically normalized audio renderer inputs. The live browser reached the bring-your-own-data importer;
HTTP checks verified the homepage, `/play/`, engine manifest/chunks, build
metadata, and a real 404. Production now exposes the same compile-gated tools
for early-user testing while retaining its separate cloud environment.
The same deployment also publishes the portable Windows runtime-test launcher
and opt-in bundled-audio fixture support in its source snapshot. The supplied
Ultima IV archive itself remains local and is not present in the deployment.

On September 15, 2026, the user explicitly confirmed removal of login, including
public access to permission-pending music. Worker-specific Access was removed
through the dashboard; it now reports **This Worker is not protected by Access**.
Unauthenticated `/play/` and a direct engine/music chunk return HTTP 200,
without an Access redirect.
Anyone who discovers the test URLs can download the site and its included assets.
This is not a private server or an artist email allowlist. `workers.dev` and
preview URLs remain disabled. Original DOS game data is never bundled; private
game-data upload requires a signed-in account and a separate explicit action.

### Initial protected setup (historical)

`wrangler.test.jsonc` uses a separate Worker `ultimatum-project-test` and
`build-test/` package. It deliberately disables `workers.dev` and preview URLs
and uses only the protected custom domain: knowing a music-chunk URL must not
bypass login. Worker-specific Access was activated before attaching the domain.
Upload confirmed by Wrangler: `3ffcd368-533b-447e-808a-66811401a9a8`.
Package preflight passed for 499 files / 59,861,278 bytes, private-review mode.
The initial upload was unrouted; the dashboard subsequently attached
`test.ultimatumproject.com` after confirming Worker Access **All traffic**.
The route is now also recorded in `wrangler.test.jsonc` for future deployments.

1. Domain added on Cloudflare Free and now active. Public DNS and the dashboard
   both confirmed the completed Namecheap nameserver activation.
   Assigned nameservers are `brady.ns.cloudflare.com` and
   `dahlia.ns.cloudflare.com`. Replace the existing Namecheap BasicDNS pair
   through Domain List > Manage > Nameservers > Custom DNS, then save.
   The scan imported one A, one CNAME, five MX and one SPF TXT record; none
   were deliberately removed. Verify any existing email/forwarding service
   before the registrar switch. Do not modify unrelated domain settings.
2. Snapshot the chosen verified preview to ignored `build-test/` without
   replacing the public or LAN preview. Run
   `node scripts/cloudflare-preflight.mjs --private-review`. This permits the
   historical artist-review music package but still verifies private-data exclusion and
   engine/source integrity. It checks files, **not** Access security.
3. Upload staging with the pinned Wrangler and `--config wrangler.test.jsonc`.
   Keep it unrouted until Access exists. Do not deploy to the production Worker.
4. Configure Cloudflare Access for **all traffic on this one Worker**. The
   initial policy allows only Cloudflare account members, with a 24-hour
   session. Artist/tester email allowlists and email one-time PIN can be added
   separately after the user approves recipients; do not grant artists access
   to the Cloudflare administration account. Do not protect every Worker in
   the account or inadvertently gate the future public site.
5. Attach `test.ultimatumproject.com` as its custom domain. Independently verify
   unauthenticated homepage, `/play/`, and direct engine/music asset requests
   require sign-in, and that an allowed reviewer can play. Verify no enabled
   alternate Worker/preview hostname bypasses that protection.
6. Share the test link only after those checks. Access limits distribution; it
   does not grant redistribution rights or prevent authorized reviewers copying.

Verified September 15, 2026: Zero Trust Free enrollment completed by the user.
Worker Access application `a6cf44f3-ca0f-4571-8f64-f2b086d60469` protects all
traffic. Unauthenticated GETs to `/`, `/play/`, the asset manifest, engine JS,
WASM, and all three engine/music chunks returned HTTP 302 to the team's
Cloudflare Access login, not asset bytes. The disabled `workers.dev` fallback
returned HTTP 404. The existing account session signed
in through Cloudflare and reached the game-data importer over HTTPS. The first
post-login load stayed on preparing; reloading reached the importer. In-game
playback and a separate artist login remain unverified on this hosted origin.
No original game data or local saves are present on a fresh origin.
Wrangler is connected using limited Worker deployment/account/DNS-read OAuth
scopes; it does not have Access-application administration scopes. Access policy
setup currently uses the dashboard. Do not broaden CLI permissions silently.

Test can retain Debug Tools; original game data stays user-supplied.
Production and staging use separate origins, so IndexedDB saves do not mix.
The production account client uses the project configured by
`ULTIMATUM_SUPABASE_PUBLIC_URL`; the QA client uses the project configured by
`ULTIMATUM_SUPABASE_TEST_URL`. Both have the matching schema and passed the
rollback-only backend suites on September 17, 2026. Production Auth uses a
six-digit OTP with a one-hour expiry, custom Resend SMTP, and the same
code-bearing email template as test. Each Supabase Site URL matches its web
origin. Build metadata, generated client configuration, and deployment preflight
all fail closed if a package targets the wrong account environment. This is an
identity/backend boundary, not browser-origin storage sharing.

References: [Workers Static Assets](https://developers.cloudflare.com/workers/static-assets/get-started/),
[custom domains](https://developers.cloudflare.com/workers/configuration/routing/custom-domains/),
[asset limits](https://developers.cloudflare.com/workers/static-assets/limits/).
