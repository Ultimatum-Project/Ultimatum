# Ultimatum U4

**Play Ultima IV: Quest of the Avatar natively on your iPhone or iPad, with
touch controls.** Ultimatum U4 is an iOS port of the `zu4` engine (a clean SDL2
fork of xu4).

The original Ultima IV DOS game data is not included in the app or this
repository. On a fresh install, the app offers to download it directly from the
[authorized Ultima Dragons mirror](https://ultima.thatfleminggent.com/u4download.html),
then validates the archive before using it. The archive is stored only in the
app's local data container.

The build also downloads the Ultima IV VGA Upgrade, verifies its checksum, and
bundles only its graphics and original readme (not its legacy MIDI drivers).

## 🚀 Install

Requires a **Mac** with **Xcode** and `cmake` (`brew install cmake`).

From the Ultimatum repository root, copy `.env.example` to the ignored
`.env.local` and set your bundle ID, Apple Team ID, and optional Supabase
publishable client configuration. Existing shell environment variables take
precedence. Signing certificates and provisioning profiles remain in Xcode and
must never be placed in this file.

**iOS Simulator** (no Apple account needed):

```sh
git clone https://github.com/dmaynard51/ultima4-ios.git
cd ultima4-ios
ios/build-ios-sim.sh          # builds a clean app and prints run commands
```

**On your iPhone/iPad** (needs a free Apple ID):

```sh
ios/build-ios-device.sh
```

That's it — it builds, signs, installs, and launches. On first run, tap
**Download Game Files** to fetch and verify the original DOS data. For a free
development profile, trust the app once under **Settings ▸ General ▸ VPN &
Device Management**.

➡️ Touch controls: [ios/CONTROLS.md](ios/CONTROLS.md)
(Find your Team ID: `security find-identity -v -p codesigning` — the code in parentheses.)

## ☕ Support this port

This iOS port is a free, open-source labor of love — porting the engine, adding
touch controls, drawing the icon, and keeping it working takes real time. If it
let you play Ultima IV on your phone and you'd like to say thanks, a coffee is
hugely appreciated (and completely optional):

- ☕ **[Buy me a coffee (Ko-fi)](https://ko-fi.com/dmaynard)**
- 💜 **[GitHub Sponsors](https://github.com/sponsors/dmaynard51)**

## TestFlight

With Xcode signed in to an Apple account that has App Store Connect access for
the selected team:

```sh
ZU4_IOS_BUILD_NUMBER=1 ios/upload-testflight.sh
```

The script creates and uploads an App Store archive named **Ultimatum U4**. It
deliberately excludes the original Ultima IV DOS game data; TestFlight installs
exercise the same first-run download as a clean local install. Increment
`ZU4_IOS_BUILD_NUMBER` for each upload. Supply the App Store bundle identifier
as `ZU4_IOS_BUNDLE_ID` through the ignored root `.env.local`.

You supply nothing but a Mac — none of the original DOS game data is included in
this repository or the distributed app.

---

Everything below is the original upstream zu4 README (desktop builds).

# Zesty Ultima IV

This is a cleaned up, modernized fork of "xu4".

### WARNING
This codebase is probably broken in a lot of ways.
I am not looking for code contributions right now,
as I am trying to realize my vision for the project
before accepting outside assistance. However, any
suggestions or bug reports are welcome.

#### How to Compile
Currently, the following dependencies are required:
libsdl2, libxml2
```
make
```

#### Where to Get Game Files
```
https://ultima.thatfleminggent.com/u4download.html
http://www.moongates.com/u4/upgrade/Upgrade.htm
```
Direct Links:
```
https://ultima.thatfleminggent.com/ultima4.zip
http://prdownloads.sourceforge.net/xu4/u4upgrad.zip?download
http://www.moongates.com/u4/upgrade/files/u4upgrad.zip
```

#### Project Goals
* Greatly simplify the codebase
* Remove all external dependencies from the engine
* Make the engine simple to plug into any framework
* Convert from ugly C++ to beautiful C
* Make it "Just Work"
