# Private artist-review graphics and audio preview

Historical verification record. Superseded September 18, 2026: the three
alternate soundtrack packs, their preparation tools, and the soundtrack
selector described below were removed. Current builds retain only the
soundtrack supplied with xu4.

Implemented 2026-09-15 at the user's request. This is a local demonstration,
not redistribution clearance. No artist messages, publication, TestFlight or
App Store upload have been performed.

## In the app

Menu → Experience → Audio provides four complete, mutually exclusive packs:
Hurin Sound Canvas, Voyager, Eric Weidenbacher MT-32, and Time Machine Dragon
Enhanced Apple II. Music and sound-effect volumes are independent. Credits
identify the composers, arrangers, recordings and pending permission status.
Native action pages are full-screen; long read-only attribution can scroll
while Back stays in its header. The browser engine uses its existing prompts.

`audio.soundtrack` is a stable global preference; missing/invalid keys retain
the original Hurin default. Profile changes and adventure saves do not own it.
Changing pack prepares all nine decoded sources before replacing the active
pack. Current track context, volume and mute are retained. Missing/corrupt
files and failed preference writes leave the old pack active. Source cleanup
uses the mixer's destructor; runtime testing also found and fixed its missing
linked-list traversal step, which could stall rapid switches.

The Enhanced Apple II pack is a **new private demo rendering**, not a copy of
the artist's original Unison playback. The nine selected MIDI files and context
mapping follow the author's MAKE.BAT; no BAT, DOS loader or uploaded executable
is executed. TinySoundFont/TinyMidiLoader (MIT) render with GeneralUser GS by
S. Christian Collins. A short edge ramp avoids clicks; this is not a promise of
perfectly seamless loops or new runtime fades (existing fades remain stubs).

## Automatic graphics, originals preserved

Native local builds continue bundling the graphics-only U4 Upgrade. Browser
builds now bundle it independently of original-data preload, so accepted game
data does not need a separate VGA upload. The optional manual overlay control
remains. EGA/Classic preferences are not reset by import.

Both targets use the same checksum-verified original upgrade and 26-member
whitelist (25 graphics assets and original readme). The output ZIP is rebuilt
fresh from verified inputs, preventing stale unexpected members. DOS music
drivers and executables are excluded. Rune images 6–8 intentionally have `.ega`
names: VGA prefers their overlay versions; EGA skips those overlay images.
Base ZIP precedence is otherwise retained; original files are not patched.

Native first-run acquisition still uses the pinned original archive download;
arbitrary native ZIP/folder import is not implemented here. Browser validated
ZIP/folder import is supported. This is not DOS 2.0 gameplay-rule parity; the
separate compatibility/rule audit in the research plan remains necessary.

## Build and permission boundaries

Default CMake packaging excludes **all** uncleared music, including previously
bundled tracks. Native builds remove stale preview music from an existing build
bundle before packaging. Local device/simulator scripts explicitly opt into
private music; web builds require `ULTIMATUM_PRIVATE_MUSIC_PREVIEW=ON`.
The TestFlight script rejects a private-preview environment and explicitly
configures music OFF. No public original-game preload is needed for the VGA
overlay. Public web deployments must also keep `ULTIMATUM_BUNDLE_U4_DATA=OFF`.

`ios/prepare-private-music.sh` fetches checksummed MIDI, renderer headers and
soundfont into an isolated cache, extracts only selected inputs/readmes, and
renders OGGs offline. Generated music and the soundfont are not checked into
the repository; the app bundles OGGs, not the soundfont or DOS scripts.
Pass the prepared directory as `ZU4_PRIVATE_MUSIC_DIR` (native) or
`ULTIMATUM_PRIVATE_MUSIC_DIR` (web).

Permission requests should cover the exact packs/renderings, public web and
iOS distribution, commercial limits, conversion/loop adjustments, and required
credit. Contributor approval does not automatically settle underlying score
rights. See [the research plan](ULTIMA_PATCHER_INTEGRATION_PLAN.md).

## Device delivery

Signed Release version 1.0 / build 17 succeeds and passes strict signature
verification. Installed over the configured development bundle on the connected
iPhone, preserving its existing app container; launch succeeds and device
inventory confirms version 1.0 / bundle version 17. The other preview bundle
is untouched. Build evidence: `/private/tmp/u4-build17-device-final.log`.
Build concurrency was reduced to two jobs after memory pressure slowed the
first attempt; existing compiled files were preserved.

## Verification

- Mobile regression suites pass, including new ZIP/loose VGA precedence tests
  and stable soundtrack persistence/migration checks.
- Host SDL/mixer tests decode and produce nonzero PCM from all 36 songs, and
  cover invalid/missing/corrupt packs, save rollback, muted switching, repeated
  and rapid context switches, cleanup and safe empty/public music startup.
- Isolated `org.ultimatumproject.tests.audio` runs the actual native engine and button
  targets: four packs/all nine contexts, picker cancellation, mute, 60% music,
  40% effects, credits, world-timer pause, unchanged moves/food and return to
  main controls. Actual engine relaunch restores all audio preferences.
- Standalone UIKit tests assert all action pages fit without scrolling and
  preserve 44-point targets at 375×667 and 852×393 with simulated safe insets.
  These are native-view geometry tests, not physical device rotation coverage.
- 27 web unit tests pass. Actual local browser prompt buttons select all four
  packs, preserve context/volume/mute, show credits, cancel/return safely, and
  persist the stable preference. Test submissions wait for current-generation
  buttons, avoiding stale prompt replies.
- Clean public web packaging contains graphics but no original data/music.
  On a separate loopback origin, validated original ZIP import boots VGA
  character creation without an upgrade upload; cancellation leaves all three
  adventure slots empty.
- Public native packaging builds successfully with the overlay and no music;
  explicit private-preview TestFlight invocation fails before archive/upload.

Temporary evidence: `/private/tmp/u4-audio-final-console.log`,
`/private/tmp/u4-audio-reload-console.log`,
`/private/tmp/u4-audio-layout-console.log`,
`/private/tmp/u4-audio-mobile-tests.log`,
`/private/tmp/u4-audio-host-tests.log`,
`/private/tmp/u4-audio-web-unit.log`, and separate public/private build logs.

Hardware listening, physical finger-touch/rotation, VoiceOver, extreme Dynamic
Type and background audio interruptions remain coverage gaps. Testing used
disposable simulator adventures and separate loopback origins, not production
adventures. Artist approval and broad-distribution review remain outstanding.
