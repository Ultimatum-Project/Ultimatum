# Ultima IV iPhone engine assessment

## Starting state

The repository initially contains only AGENTS.md. Xcode 26.3, Swift 6.2.4,
and CMake are installed on the development Mac.

AGENTS.md requires reading `docs/design/PC_to_iOS_Interface_Playbook.md`
before mobile interface planning or implementation. The user supplied the document at
`~/Documents/PC_to_iOS_Interface_Playbook.md`. It has been read in full and
copied to the canonical repository path. Interface work is now unblocked.

## Candidate engine

- Official xu4: https://github.com/xu4-engine/u4
- Existing native iOS adaptation: https://github.com/dmaynard51/ultima4-ios
- Inspected adaptation revision: `52b241098d06177debdac84c72d925edc89e03e9`
- Local research checkout: `/private/tmp/ultima4-ios-reference`

The adaptation uses the zu4 fork of xu4, SDL2, C/C++, and an Objective-C++ iOS
integration. Its CMake project and simulator/device build scripts are present.
COPYING contains GPL version 2; preserve upstream attribution and review source
headers when importing. Original game data is separate from engine source.

The source includes world traversal, conversations, combat, party progression,
shrines, dungeons, save serialization, and the Codex ending. The ending checks
include the three-part key, eight party members, and all eight virtues. These
are code observations, not proof that this iOS build can be completed.

## Engineering findings to verify

- Simulator access from the restricted shell failed with CoreSimulator service
  permission/connection errors. Retry with the appropriate runtime access.
- Upstream scripts write into ~/Library/Caches, download game data over HTTP,
  and delete/rebuild staging directories. Inspect and adapt these before use.
- The game-data predicate needs explicit grouping: both AVATAR.EXE and
  TITLE.EXE must be required regardless of filename case.
- SDL is pinned to 2.30.10 but upstream download scripts do not validate hashes.
- Lifecycle interruption and save recovery need direct validation; no claim
  of safe background saving has been established.

## Completion evidence required

A simulator build and launch; an actual iPhone build; new-game creation;
exploration, conversation, combat, inventory and spell use; save/relaunch and
interruption recovery; and an endgame regression using a documented fixture.
Sharing requires reproducible build instructions, upstream notices, a clear
original-data setup, and a device-signing path. A successful compile alone
must not be described as a polished or completable port.
