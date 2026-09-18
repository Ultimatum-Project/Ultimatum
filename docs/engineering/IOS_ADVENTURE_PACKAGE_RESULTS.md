# Native iOS adventure import/export

Implemented September 15, 2026. Local transfer only; no cloud service or account
credentials are used. Native `.u4save` JSON is the existing web version-1 format.

## Delivery and safeguards

Native Files import and share-sheet export are available from the game pause
menu and the Journey/New adventure slot picker. Every import passes bounded
JSON/base64/member-count checks, exact allowlisted filenames, declared size and
CRC32 checks, native core save/dungeon and journal/notebook/map validation before
slot publication. Invalid transport, moon, orientation and status values are
rejected rather than reaching unsafe engine initialization.

Occupied-slot replacement is confirmed and uses immutable checkpoint generations
with CURRENT/PREVIOUS recovery. Pre-generation Slot 1 is copied into a recovery
generation before replacement; its original files are not removed. Import does
not change the selected slot and cannot replace the running adventure. The
source backup is never changed. World time/input is paused through system sheets.

Export explicitly distinguishes current safe-to-save progress from a saved
checkpoint. Game data, device preferences and unfinished combat are not included.
All applicable portable files include native notes/favorites and explored maps,
pins and discoveries. An imported web conversation transcript is retained across
the next native save and included in durability/retention handling.

## Verification

- Native Foundation codec/slot test runner: **141 checks passed**.
- Actual native codec → web codec note/map edits → native validated slot
  installation → web decode: passed for world and dungeon fixtures.
- Real browser engine on an isolated loopback origin: native package import
  through the web file handler, Continue at 9,471 moves, Journal bookmark,
  Unicode/multiline attached note and standalone note display, new web note,
  actual checkpoint save and portable export: **3 runtime groups passed**.
- That real browser export passed native decoding, validation and independent
  slot installation.
- Separate native simulator bundle: actual UIKit Files/share presentation,
  cancellation, malformed file rejection, active-slot exclusion, destination
  replacement Back, confirmed replacement with previous recovery, portable
  share file validation, cancelled sharing and return to game controls passed.
  Moves, food and wind stayed unchanged while transferring.
- Actual native engine relaunch loaded that browser-exported adventure. The
  next actual native save passed validation and preserved the notebook and
  web transcript byte-for-byte.
- Existing mobile regression suites passed.
- The user additionally confirmed a successful iOS → mobile web → iOS round
  trip on their actual devices.
- Full portrait native transfer flow and Journey slot-picker cancellation passed;
  every exercised transfer action fit the panel safe area with a 44-point target.

The automated simulator test supplies document URLs to the real picker delegate
and invokes the real share completion callback. It does not claim physical
finger-touch coverage, third-party/iCloud file-provider downloading, or external
share-destination completion. No user adventure was imported or replaced during
testing.

## Signed-device delivery

Signed device Release **1.0 / build 18** passed building and strict signature
verification and was installed over the configured development bundle on the connected
iPhone, preserving its data container (no uninstall/reset/import). Device app
inventory confirmed version 1.0 and bundle version 18 after installation.
Automatic launch was attempted but iOS denied it because the phone was locked;
launch/physical-device interaction remains pending unlock. No TestFlight or
App Store upload was performed.

## Pause-menu height regression follow-up

The new backups action exposed an existing fixed 260-point compact-menu cap:
the fourth row was clipped on the device with scrolling intentionally disabled.
Compact menus now measure the complete stack at their actual width and grow
upward within safe bounds. Dense landscape menus reclaim width for five or
more rows. Ordinary menus remain entirely visible without scrolling; overflow
on extreme text/short windows is scrollable rather than inaccessible.

Twelve standalone UIKit cases passed (7–9 actions on 375×667, 393×852,
852×393 and 768×1024). Every action was wholly visible with 44-point targets
and no scrolling. The running isolated engine also passed native UI clicks
through the last-row Adventure backups action, Back and Resume, preserving
resources and returning input to gameplay. Existing mobile suites passed.

The exact seven-action Debug Tools menu was visually verified fully visible in
the real engine in landscape and upright portrait, including rotation while
open. Backups → Back → Resume passed again in portrait. Signed device Release
**1.0 / build 19** passed signature verification and installation over the
existing iPhone app without resetting its data. Post-install device launch and
inventory checks encountered intermittent network/tunnel failures; final
device app inventory subsequently confirmed version 1.0 / bundle version 19
using the refreshed hardware identifier. Post-install automatic launch is still
subject to the device connection/unlock state; no physical interaction with the
user's adventure was performed.
