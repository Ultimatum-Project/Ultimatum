# Trying upstream Ultima IV on iPhone

Source: https://github.com/dmaynard51/ultima4-ios
Revision: 52b241098d06177debdac84c72d925edc89e03e9
Local source: vendor/ultima4-ios

This is an upstream baseline trial, not the planned redesigned mobile port.
Gameplay and touch UI are unchanged. The additional build-local-device.sh
uses /private/tmp/ultima4-device-build and stops after signing so installation
can explicitly target the iPhone rather than the first paired device.

Bundle identifier: configured private preview bundle
Device: iPhone 14 Pro

## Controls

Hold the phone landscape. Use the D-pad to move. Tap the keyboard button
for letter commands: T talk, L look, E enter, A attack, C cast, Z statistics.
Return advances text; Esc backs out. Space passes a turn.
Conversations use typed keywords such as name, job, health, and bye.
Use Q outdoors and check for the save confirmation before closing the app.
Although upstream documentation claims automatic saving, the inspected engine
save call is the Q command; lifecycle autosaving has not been verified.

## Useful trial

Create a character, enter a town, speak with someone, inspect party statistics,
return outdoors, save, close and reopen. Check text size, keyboard obstruction,
D-pad comfort, and the number of steps needed to issue common commands.

Build log: /private/tmp/ultima4-device-build.log
App output: /private/tmp/ultima4-device-build/zu4-device/Release-iphoneos/zu4.app

## Installation result — September 10, 2026

Release iPhone build and development signing succeeded. `devicectl` confirmed
installation and launch on the paired iPhone 14 Pro with bundle identifier
the configured private preview bundle. Gameplay usability remains for on-device trial.

The downloaded DOS files were read-only, which caused upstream's post-build
`xattr` command to fail. Granting the owner write permission on the extracted
data and copied app bundle resolved the failure; the subsequent build passed.
Successful build log: /private/tmp/ultima4-device-rebuild.log
