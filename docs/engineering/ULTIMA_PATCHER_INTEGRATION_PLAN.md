# Pix's Ultima Patcher: asset permissions and integration plan

Reviewed 2026-09-15. The private artist-review implementation is recorded in
`ULTIMA_PATCHER_PRIVATE_PREVIEW_RESULTS.md`; the research and original plan
below remain useful background. Neither implementation nor attribution clears
every asset for redistribution or authorizes publication. This is a project
assessment, not a legal opinion. No permission requests have been sent.

Update 2026-09-18: the Voyager, MT-32, and generated Time Machine alternatives,
their preparation tools, and the soundtrack picker were removed. Current web
and iOS builds retain only the soundtrack supplied with xu4. The switching
design below is historical and must not be re-enabled without a new rights and
packaging review.

## What would actually replace the existing VGA upgrade?

Nothing different visually. All 26 members of the graphics-only package made
by `vendor/ultima4-ios/ios/prepare-vga-upgrade.sh` are byte-identical to files in
Pix's `UltimaPatcher/Files/U4VGAUpgrade` directory: 25 assets and `Readme.txt`.
This was checked by comparing the Git blob SHA-1 of each uncompressed local
ZIP member against the GitHub recursive tree, not archive filenames or dates.
The reviewed patcher tree SHA is
`c5db7b77b93a3b99b27556c3e5d8654d1679ced5`.

The cached original upgrade SHA-256 is
`400ac37311f3be74c1b2d7836561b2ead2b146f5162586865b0f4881225cca58`.
Pix is an installer/collection of independent upgrades, not a newer unified
VGA renderer. Its Windows/VB installer invokes DOSBox and copies/patches DOS
files. Our xu4-derived engine implements rendering and gameplay natively.
Do not execute the installer, its BAT files, or uploaded executables.

The correct automatic behavior is to attach our verified asset overlay after
accepted original game data becomes available. Preserve the original data and
existing settings. No patch-on-patch modification of the user's archive is
necessary. Already-patched DOS installations need their own reviewed
compatibility profiles; they must not bypass existing validation.

Sources: [patcher source](https://github.com/Fenyx4/UltimaPatcher/blob/master/UltimaPatcher/U4Form.vb),
[VGA package](https://github.com/Fenyx4/UltimaPatcher/tree/master/UltimaPatcher/Files/U4VGAUpgrade),
[upgrade project](https://www.moongates.com/u4/upgrade/Upgrade.htm).

## Redistribution evidence and limits

| Component | Evidence found | Project disposition |
| --- | --- | --- |
| Pix's installer code | Root MIT license, copyright 2020 richardpickles123 | Reuse code only with its required copyright/license notice; this does not erase separate third-party terms. |
| U4 Upgrade author's software | Package readme says its software may be distributed freely; specifically: “software may be distributed to anyone, anytime, with no restrictions.” | Positive express redistribution evidence for the author's contribution. Retain original readme. |
| Upgrade graphics | Project website explicitly permits graphics use in other projects. | Positive graphics reuse evidence. Use the existing graphics-only whitelist and credit Wiltshire Dragon/Joshua Steele and Aradindae Dragon/Ryan Wiener. This is not blanket clearance of original Origin material. |
| MIDPAK and associated DOS drivers | Readme explicitly separates their terms and requires registration for certain commercial uses. | Exclude. Our mixer does not need these programs or drivers. |
| Time Machine Dragon enhanced music | Author's page and archive explain modification/use; Minstrel readme separately reserves Origin and MIDI-arranger copyright. No general redistribution license found in the inspected material. | Permission unresolved for bundling and for distributing new rendered recordings. |
| Voyager Dragon music | Archive provides xu4 installation instructions and OGG recordings, but no express general redistribution license. Newer xu4 module exists as well. | Permission unresolved for our public distribution; current inclusion in another engine is evidence of availability, not a general license. |
| Hurin/Telavar music, already bundled | Hurin publicly welcomes xu4 module distribution. The included Telavar readme reserves Origin/composer rights. | Positive author-specific xu4 evidence, not complete clearance for Ultimatum, the arrangement, and underlying compositions. |
| MT-32 music, already bundled | xu4 maintainer reports Cloudschatze gave permission to create an xu4 module. Our nine files are not byte-identical to that module. | Do not assume that module permission covers these exact files or our distribution; confirm recording identity and intended scope. |
| Original game bytes / complete patched installations | Separate original-game rights; see `ULTIMA_IV_REDISTRIBUTION_NOTE.md`. | Continue bring-your-own-data for public web gameplay. Do not ship whole DOS installations or replacement original TLK/EXE files on the basis of the patch author's license. |

Sources: [MIT license](https://github.com/Fenyx4/UltimaPatcher/blob/master/LICENSE),
[upgrade readme](https://github.com/Fenyx4/UltimaPatcher/blob/master/UltimaPatcher/Files/U4VGAUpgrade/Readme.txt),
[graphics reuse statement](https://www.moongates.com/u4/upgrade/Upgrade.htm),
[Time Machine's original page](https://sacredbacon.com/ultima/),
[Voyager downloads](https://exodus.voyd.net/downloads/),
[Hurin readme](https://github.com/xu4-engine/U4-Hurin),
[xu4 music-module discussion](https://sourceforge.net/p/xu4/discussion/169401/thread/54d7f9ddb1/).

Attribution is appropriate but is not itself a permission grant. Music may
involve separate compositions, arrangements, recordings and soundfont terms.
Author approval of a recording does not automatically settle every layer.
The [U.S. Copyright Office FAQ](https://www.copyright.gov/help/faq/faq-fairuse.html)
recommends obtaining permission where authorization is uncertain.

For soundtrack inclusion, obtain a durable statement covering the exact
files, public web hosting, iOS bundling/TestFlight/App Store distribution,
format conversion/loop edits, required credit, and any commercial-use limits.
Ask what rights the contributor holds and what additional rights remain.
This review does not silently remove existing development assets, but their
presence must not be presented as proof of public-release clearance.

## Two alternatives, plus our existing soundtracks

Time Machine Dragon's package contains nine MIDI selections and a replacement
`LARGE.XMI`. It is based on Minstrel's Apple II extractions, with added voices,
changed instrumentation, and an extended combat track. It is not an OGG pack.
`MAKE.BAT` establishes the actual game-context mapping:

| Engine context | Enhanced MIDI |
| --- | --- |
| World | `u8fix.mid` |
| Town | `u9fix.mid` |
| Shrine | `u4fix.mid` |
| Shopping | `u5fix.mid` |
| Rule Britannia | `u1.mid` |
| Fanfare | `u7fix.mid` |
| Dungeon | `u6fix.mid` |
| Combat | `u3long.mid` |
| Castle | `u2fix.mid` |

Its original `u4mcs.zip` SHA-256 is
`f7bec3b66a7c09336336c8c593122909c93fa9ff7ed8f9129fe3b14525b69339`.
Downloaded from [the author's archive](https://sacredbacon.com/ultima/u4mcs.zip)
into isolated temporary research storage; no included scripts were executed.

Voyager's 1.1 archive includes nine xu4 OGG tracks plus DOS XMI/COM files.
The superseded preview implementation bundled its xu4-compatible OGG set under
`music/voyd`; that directory is no longer present.
The inspected [original archive](https://bitbucket.org/mcmagi/ultima-exodus/downloads/u4-voyd-music-11.zip)
SHA-256 is
`8dda2c30753fc7a5a27bc3b00cb79f48191021b678fc2cd04dc698e892b65473`.
Do not copy the replacement `ULTIMA.COM`: it serves the DOS music loader.

At the time of this review, the app configuration played Hurin's Sound Canvas
recordings and also bundled `music/mt32`. The MT-32 directory and its iOS/web
packaging rules have since been removed.
This is separate from the graphics-only VGA upgrade. Neither two alternative
music patches should be stacked: select one complete soundtrack at a time.

## Proposed implementation

Follow the canonical PC-to-iOS playbook: settings in temporary panels,
finger-sized explicit choices, clear selection state, safe cancellation,
and preserved lifecycle state. No playbook deviation is proposed.

### 1. Automatic graphics overlay after validated import

- Maintain one versioned, checksum-verified, graphics-only upgrade package
  shared by the native and browser builds, with original readme/credits.
- iOS already bundles it. The native first-run flow currently downloads one
  pinned original archive; arbitrary user ZIP/folder import is not implemented.
  Add native import through the same reviewed per-file profiles before
  describing upload parity as complete.
- Public web currently requires a separate VGA import. Bundle the reviewed
  graphics-only overlay independently of `ULTIMATUM_BUNDLE_U4_DATA` and attach
  it automatically after successful original-data validation. Remove the need
  for a separate patch upload for this standard pack. Do not accidentally
  preload original game data when enabling this overlay.
- Keep user original files immutable. Persist base profile and applied overlay
  ID/version separately. Reapplying the same overlay is idempotent; failures
  preserve the previous library and saves.
- On a new Ultimatum install, use VGA by default. Existing EGA/Classic choices
  remain respected; importing replacement game data must not reset settings.
- Define asset-specific precedence rather than globally putting a patch ZIP
  ahead of all originals. Current native `u4fopen` searches the base ZIP first.
  VGA rune images 6–8 share `.ega` filenames with base images; test that these
  resolve from the overlay only while VGA is selected. EGA must retain the
  original versions. Use the same explicit mapping on web.
- During a running adventure, stage data/overlay replacement for reload;
  do not replace assets under the active engine or alter its save.

### 2. Native soundtrack switching and settings (historical; superseded)

- Add a stable global preference such as `audio.soundtrack=hurin|voyager|mt32|timemachine`;
  preserve Hurin for existing installs and migrate missing/invalid values
  safely. Keep this independent of adventure saves and experience profiles.
- Create a pack catalog with labels, context paths, attribution, provenance,
  availability and reviewed distribution status. Public packaging includes
  only packs cleared for that distribution; unavailable packs must not cause
  crashes or silent library replacement.
- In the existing Menu > Experience hub, add an Audio panel with Soundtrack,
  Music volume, Sound effects volume and Music credits. Use a short full-screen
  choice list with a checkmark, descriptive subtitle, and Back. Reuse the
  native topic-panel/web-menu adapters; no permanent HUD controls are needed.
- Selection loads/validates all nine tracks before changing the active pack.
  Persist successfully, swap on the engine thread, destroy old sources with
  `cm_destroy_source` (not raw `free`), and resume the current context track.
  A failed load or preference write leaves the old soundtrack active. Retain
  volume and mute state, including when changing packs while music is muted.
- Do not restart SDL audio or disrupt sound effects. Test mixer concurrency
  and repeated switches for memory leaks. Smooth fades are a separate task:
  the current music fade functions are stop/play stubs, not implemented fades.
- Our mixer supports OGG/WAV, not MIDI/XMI. Once permissions are resolved,
  pre-render Time Machine's MIDI selections using a soundfont with suitable
  terms and verified loop points. Do not add DOSBox/MIDPAK or execute BAT
  compilation scripts inside the app. MIDI rendering is a build/asset task,
  not something performed on users' game uploads.

### 3. Gameplay/data corrections: a distinct audit

Audit the non-visual patches separately. Native Hythloth room/start-position
corrections and custom keyword precedence already exist. Compare the
remaining conversation question triggers and Serpent's Hold guard IDs against
the reviewed 1.01 data profile. Prefer small guarded in-memory corrections
with original-byte checks to distributing entire original TLK/ULT files.
Do not silently adopt DOS 2.0 balance/rule changes such as diagonal attacks.

### Verification before delivery

Portable tests: settings migration/invalid IDs, complete pack mapping,
missing/corrupt tracks, rollback, overlay whitelist and provenance.
Runtime tests in isolated adventures: import/reimport/rejection, retained
save/journal data, EGA/VGA toggle and rune/cutscene decoding, soundtrack changes
in world/town/shop/shrine/dungeon/combat/fanfare contexts, mute and volumes,
cancel/back/resume, repeated switches, app restart, background/resume,
portrait/landscape and VoiceOver. Test both archive and loose-file base data.
After relevant tests and a signed device Release build succeed, follow
AGENTS.md's existing automatic iPhone install/version verification workflow.

## Recommended order

1. Automatic graphics-only overlay and provenance/credits.
2. Retain only the soundtrack supplied with xu4; do not expose alternate packs.
3. Reconsider alternate soundtrack packaging only after permission scope is
   resolved and a new explicit product decision is recorded.
4. Guarded remaining conversation/data fixes after separate compatibility QA.

No app build, device installation, account upload, or public deployment was
performed as part of this research/planning turn.
