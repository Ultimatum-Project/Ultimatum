# Game-data compatibility and private storage

## Implemented: browser-local Ultima IV verification

`clients/web/dist/game-data-manifest.js` describes the reviewed English DOS/EGA
profile `u4-dos-english-ega-v1`: 103 required filenames, lengths and SHA-256
fingerprints, derived from the bundled engine's map/dialogue/EGA configuration.
It contains fingerprints, not original game bytes. A contract test catches
configuration changes that require updating the compatibility profile.

The additional `u4-dos-english-ega-1.01-v1` profile recognizes the documented
seven-byte data-fix variation: COVE.TLK, LCB.TLK, MINOC.TLK, SKARA.TLK and YEW.TLK
change one byte each; SERPENT.ULT changes two guard conversation identifiers.
All other 97 required files, including both executables and EGA images, match
the initial profile. A complete set must match one profile; accepting a mix of
individually known hashes would hide accidental version mixing and is rejected.

Imports preserve and group normalized directory paths before selecting files.
Exactly one directory containing all 103 required filenames is selected;
partial nested upgrade folders are excluded. Multiple complete directories are
reported as ambiguous, not silently selected or merged. With no complete set,
the largest candidate produces missing-file guidance; tied partial candidates
require the user to choose one folder. Duplicate filenames within the same
normalized, case-insensitive directory remain errors. Neither an archive's
name, version text nor an `upgrade` folder name grants compatibility.

Folder imports filter before reading: unrelated documents/programs/media and
embedded saves are not included. The ZIP staging bridge checks supported
entries, unsafe paths, duplicate case-insensitive game paths, extraction
CRC, 2 MiB per selected file, 32 MiB unpacked selected data, 2,048 archive entries,
and 128 MiB compressed input. Unrelated archive members are not decompressed.
Direct URL imports enforce the compressed limit while reading the response,
including when Content-Length is absent or wrong. Fetching remains browser
CORS-based; there is no server-side URL downloader.

The complete selected set must match the profile's file sizes and SHA-256.
This is intentionally stricter than structural plausibility: the engine reads
fixed-offset strings from executables and uses legacy asset decoders. Uploaded
DOS executables are data sources only; they are never executed. Dialogue record
string boundaries/nonempty descriptions and town-person coordinates/movement
are also checked. Some original conversation IDs exceed the dialogue count;
the native loader safely leaves those unmatched, so they are not rejected.

Unrecognized modified or translated data, other platform editions, and changed compressed
images are rejected with an unsupported-version message. Supporting another
valid release means adding a reviewed profile and testing it end to end,
not weakening the checks or treating a new hash as automatically trusted.

The initial VGA profile verifies the exact known 374,307-byte U4UPGRAD.ZIP by
SHA-256 before retaining/installing it. Repacked ZIPs with identical extracted
content are currently rejected; per-file overlay profiles are a future
extension. Arbitrary ZIPs cannot replace the overlay.

Validation uses isolated staging, not the active `/ultima4` directory. Before
startup, accepted files are written into a new directory and renamed into
place; the old directory remains until the IndexedDB transaction commits.
A failed write leaves active data in place; a failed durable commit rolls the
directory back. During gameplay accepted replacements are retained for reload
without changing live engine files. Restore rehashes stored file bytes rather
than trusting stored hashes. Adventure records are separate and untouched by
game-data imports. Legacy loose-file embedded saves can migrate only once,
if no working save exists and native checkpoint validation succeeds.

Hosted HTTPS uses Web Crypto. The HTTP phone LAN preview has a tested software
SHA-256 compatibility fallback, used for file fingerprints only, not secrets,
authentication or signing. Import controls are disabled while an import runs.

These checks establish compatibility with a reviewed byte set. They do not
establish ownership/licensing, authenticate an account, provide a malware
scanner, or certify the security of every legacy engine parser.

## Future: account service and deduplication

Keep the game identifier, compatibility profile and logical file manifest as
the account library's metadata. Each account owns a private manifest/reference;
identical immutable file bytes may share one physical object behind the API.
Count logical bytes against the user's 100 MB allowance regardless of how
many accounts share an object. Protect save capacity separately.

The server must revalidate uploaded bytes and recompute every SHA-256. Client
hashes and a passed browser check are not authorization. Do not grant access
from a claimed hash, expose cross-account existence probes, or skip upload
based on another user's object. Downloads require ownership of an account
reference; storage is private and signed URLs must be short-lived. Replacements
create new immutable objects; deletion drops only that account's reference.
Garbage collection should use authoritative live references with a grace
period, transactional attachment/deletion, and protection against concurrent
uploads—not an unguarded reference counter.

Persist canonical accepted files, not the original ZIP or unrelated documents.
Validate quotas before reference publication; limit staging lifetime and
resource use. Keep checkpoints account-specific initially. Full server-side
validation, ownership, quota enforcement, deduplication, garbage collection,
cloud sync and native iOS adoption of these compatibility profiles are not
implemented in this slice.
