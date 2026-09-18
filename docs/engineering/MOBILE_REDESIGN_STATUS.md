# Mobile redesign status

## Current verification summary

This is an unfinished simulator prototype. The phone still has the upstream
baseline. Both current arm64 simulator and unsigned physical-iPhone targets
compile successfully (integrated build logs in /private/tmp/ultima4-integrated-
{sim,device}-build.log). Compilation does not establish mobile playability.

Earlier simulator sessions verified ordinary conversation discovery, journal
reload, conversation rotation, equipment changes, and mixing. Recent save,
turn-accounting, direction, exploration, quest-item, stone, virtue, and mantra
flows still require interaction tests. Computer Use again reported the Mac
locked during the latest integration check.

Release blockers include native onboarding, vendor and combat flows, final
Codex prompts, full adaptive layout review, interruption-safe saving, end-to-end
completion, physical-phone testing, and a shareable distribution build.

The sections below are a chronological engineering log. Later entries supersede
earlier implementation and installation claims.

## Accepted direction

Build on the installed ultima4-ios/zu4 foundation. Replace the desktop UI with
native mobile status, conversation, journal, inventory/equipment and spell
panels. Compose portrait and landscape separately and retain interaction state
on rotation. Typing is reserved for a player name and deliberate custom input.
The canonical design standard is ../design/PC_to_iOS_Interface_Playbook.md.

Conversation choices must come from topics the player has encountered, plus
ordinary introductory choices. Keep a custom-keyword path. Never discover
secrets by scanning unread response content. Preserve U4 mechanics and ending.

## Implemented, not yet wired into gameplay

src/topicjournal.h and src/topicjournal.cpp provide a portable exposure record,
source labels, parser-prefix lookup, and atomic file replacement. Failed or
malformed loads leave existing memory untouched. New-game reset is explicit.
Dialogue now exposes keyword metadata without evaluating responses. The journal
filters that metadata into ordinary defaults and discovered topic choices,
with readable labels, duplicate removal, and exclusion of the OJNA easter egg.
No game hook calls this component yet; it does not change the installed app.

Standalone tests passed with clang++ C++14, -Wall -Wextra -Werror. Coverage:
unseen topics, normalization, four-character topic lookup, short-keyword false
positives, duplicate exposure, multiple sources, persistence, corrupt input,
reset, undiscovered-topic filtering, ordinary defaults, and choice labels. Tests are in vendor/ultima4-ios/tests/topicjournal_test.cpp.

## Next integration

1. Define NPC valid-topic access without evaluating responses or their effects.
2. Hook displayed dialogue chunks, then signs/books; avoid recording unseen
   queued chunks. Handle line-wrap splits and parser aliases explicitly.
3. Add safe ordinary-topic defaults and a curated secret-answer exclusion policy.
4. Bind journal load/reset/save to the active adventure; test new game and resume.
5. Add a conversation bridge with semantic input submission and visible choices.
6. Implement adaptive world/panel composition and verify actual rotation.
7. Extend semantic choices to all remaining prompts; test combat and ending.

The overall port remains incomplete. Engine compilation and an upstream device
launch are established; redesigned gameplay and completion are not yet verified.

## Integration update

The iOS engine now observes each displayed conversation reply chunk, persists
it to topics.txt in the current settings/save directory, and reloads it when
initializing gameplay. New-game creation removes the previous topic file.
Source labels use the location rather than silently revealing the NPC name.
The source is included in CMake and C++14 is explicitly selected.

The unsigned arm64 iPhone target built successfully after Xcode regenerated its
source list. Log: /private/tmp/ultima4-topics-build.log. This build has not been
installed. Runtime discovery, save/reset behavior, final conversation chunks,
text wrapping, signs/books, and topic UI still need integration verification.
Earlier statements above that no game hook exists are superseded by this update.

## Native conversation panel update

A native UIKit conversation panel is now wired into ordinary TALK-state string
input. It displays the presented reply text, ordinary/discovered valid topic
buttons, and an explicit custom keyword field. A WaitableController receives
the selected keyword directly; no synthetic text keystrokes are used. The
panel has Dynamic Type text, a scrollable layout, and minimum 48-point buttons.
Keyword metadata access is now available through Person's dialogue accessor.

Unsigned arm64 iPhone compilation passed: /private/tmp/ultima4-panel-build.log.
This proves compilation only. The panel is not yet installed or visually/runtime
verified. Remaining work includes custom-keyboard avoidance, dialogue paging,
yes/no and vendor prompts, transcript history, rotation and world composition.
The existing app orientation restriction is unchanged; adaptive panel constraints
alone do not establish portrait support. Do not describe this as mobile-ready.

## Dialogue flow update

Native Continue panels now handle reply paging and final replies; ordinary
ASK/ASKYESNO prompts use Yes/No buttons. Final presented replies are also
recorded in discovery. Topic panels use ARC for lifecycle management and the
iOS 15 keyboard layout guide for custom-entry visibility. Older iOS keyboard
avoidance remains pending. The regenerated iPhone target compiled successfully.

Simulator build started for visual/runtime validation. Active exec session:
52846. Log: /private/tmp/ultima4-simulator-build.log. Poll that session before
starting another simulator build. Phone installation is unchanged.

## Simulator preparation and discovery regression

An iPhone 14 Pro simulator is booted
and selected in Simulator. The pre-existing iPhone 16e simulator was left alone.
Simulator build session 52846 remains live, progressing through SDL's first-time
platform checks. No runtime claim has been established; continue polling it.

Full-length keywords now require full-word exposure, preventing e.g. HUMILITY
from being discovered through HUMID. Four-character engine abbreviations retain
prefix matching and still need curated disambiguation. Standalone tests passed.

## Simulator launch result

Simulator build session 52846 completed successfully. Installed and launched
the configured preview bundle on a dedicated iPhone 14 Pro simulator. The title animation and main menu were
visually observed in landscape. The new conversation panel is NOT yet verified:
keyboard attempts did not advance from the title menu, then Computer Use
reported transient noWindowsAvailable. Rebinding com.apple.iphonesimulator
returned a window title but no controls. Recheck UI state before interaction.

The simulator data container was inspected through `simctl get_app_container`.
Documents contained no save files when inspected. A simulator-only seeded save
and --skip-intro launch can provide a reproducible gameplay fixture. Do not
modify the physical phone's saves.

Native iOS dialogue now preserves each whole response instead of dividing it
by the legacy DOS text-window dimensions. Desktop paging remains unchanged.
This change was included in the successful simulator build.

## Gameplay fixture verification

Copied the bundled PARTY.SAV into the previously empty simulator Documents/.xu4
folder as party.sav and launched with --skip-intro. The world loaded with the
bundled character Lindsay. Touch movement was visually verified. East, South,
then the software keyboard's E key entered Jhelom. Two East moves and one North
move placed the character immediately south of a townsperson in the inn area.
No in-game save has been issued for this moved position.

The keyboard causes transient AX noWindowsAvailable, and subsequent AX calls
can select the other simulator window. Always verify the iPhone 14 Pro window
before input. The native conversation panel has still not been reached. A
visible semantic Talk control remains the appropriate product work and will
also remove this dependency on unreliable legacy keyboard navigation.

## Verified native dialogue and discovered topics

Added visible Enter and Talk actions using the existing exploration command
dispatcher directly (no synthesized keyboard events). Both reject input when
the exploration controller is not active. Existing Talk direction/range and
turn accounting remain in the dispatcher. These temporary controls still need
integration into the final adaptive HUD.

On the iPhone 14 Pro simulator, verified by actual touch:
- East, South, Enter enters Jhelom.
- East twice, North, Talk, North opens the stern guard's native panel.
- Job produces the guard's response about gates and passages.
- Gates and Passages then become selectable topics.
- Gates returns "The gates of Jhelom."
- topics.txt contains both discoveries with source "Conversation in Jhelom".

The simulator container changed after update; query `simctl get_app_container`
again after any reinstall rather than assuming
its UUID remains stable.

Visual review found excessive inherited blank lines, DOS line wraps, and tall
single-column topic rows. The next compiled build removes blank paragraphs,
joins intra-paragraph wraps, uses two-column topic rows, and puts Name/Job/Health
before discoveries with Goodbye last. Topic tests and simulator build passed.
These latest layout fixes are compiled but not installed/visually verified yet.
The simulator remains in the guard conversation with the preceding layout.

## Verified portrait and rotation foundation

Enabled portrait in Info.plist and SDL_HINT_ORIENTATIONS, and added safe-area
control relayout without recreating input controls. Simulator build passed.
Verified actual portrait movement, Enter, Talk, and native topic selection.
Opened the guard conversation in portrait, selected Gates, then rotated to
landscape: "The gates of Jhelom" and the topic choices remained intact.
Previously discovered Gates/Passages also survived app replacement/relaunch.

The revised conversation layout was visually verified in both orientations:
readable unwrapped text, two-column topic rows, and defaults before discoveries.
The world is still the original desktop framebuffer centered in portrait and
is too small. Separate world/status/journal composition, complete semantic
commands, dynamic availability, and full accessibility remain substantial work.
The simulator is currently in the landscape guard conversation. Physical phone
installation is unchanged. This is not a claim that the whole port is finished.

## Separated world and native exploration HUD

Normal exploration now renders only the 176x176 world region into a device-sized
viewport. Native labels show avatar health/resources and recent game messages.
Portrait uses a large upper map, messages below it, and bottom controls;
landscape centers the map between controls with messages at left. Non-exploration
controllers retain the legacy full framebuffer until their semantic panels exist.
This preserves menu/prompt access during incremental integration but causes
layout switching and is not final polish.

Visually verified the larger portrait map and native status/messages, plus the
landscape composition, on the iPhone 14 Pro simulator. Subsequent compiled header
change adds a Britannia fallback location and moon/wind information. Screen-message
capture excludes recursive desktop wrapping calls and keeps bounded recent text.
This is a recent-message label, not the requested persistent browsable journal.

Pending: native inventory/gear/spells/party panels, complete prompt coverage,
contextual action availability, full touch mapping for world taps, autosave and
interruption handling, polished orientation-specific composition, endgame tests,
and distribution. World tap coordinate mapping must be updated for the cropped
viewport before it can be used as a supported interaction. Direction controls
remain the supported movement path. Physical phone installation is unchanged.

## Persistent browsable journal

TopicJournal format v2 stores complete presented replies alongside exposed words
and sources, deduplicating repeated text/source pairs. The loader accepts legacy
v1 topic-only files; it cannot reconstruct historical text absent from those
files. Multiline/quoted transcript persistence tests and simulator builds passed.

A visible Journal action opens paged entries and entry details through the native
panel without advancing an exploration command. Runtime verification on iPhone
14 Pro simulator: talk to Jhelom guard, ask Job, Goodbye, Continue, Journal;
then reinstall/relaunch, reopen Journal, and read the complete saved gates/passages
reply. Transcript and source survived relaunch.

Visual testing caught overlapping multiline labels in journal rows. Fixed with
full-width rows for long labels and a UIButton subclass whose intrinsic height
follows wrapped text. Rebuilt, reinstalled, and visually verified readable list
and detail views. Simulator currently shows that journal detail in portrait.

Journal polish remains: group exchanges by speaker/session, omit trivial farewells
and legacy prompts from previews, add search/notes and explicit discovered-topic
navigation, and collect signs/books. This is useful persistent dialogue history,
not completion of all planned journal features. Physical phone unchanged.

## Native party/equipment foundation

Added Party action, member selection, character statistics/current gear, and
weapon/armour selection. Lists use real inventory counts and class restrictions.
Mutations call PartyMember::setWeapon/setArmor, preserving return/consume rules,
then close the panel and finish one exploration turn. Browsing/canceling does
not explicitly spend a turn. Combat equipment UI remains pending.

Simulator runtime verified: Lindsay starts with Axe/Leather; choose Hands;
party details show Hands; weapon list then shows Axe — 1 spare; equip Axe and
receive the correct confirmation. The engine build passed. Latest subsequent
compiled refinement separates stat paragraphs and omits already-equipped items
to avoid spending a turn on an unchanged selection; that refinement is not yet
installed. Armour flow compiled but has not yet had the same runtime round-trip.

Remaining party work includes conditions, level details, richer item comparisons,
combat access, full eight-member tests, save/reload equipment verification, and
integration into the final HUD rather than the temporary button stack.
Physical phone installation is unchanged. Simulator currently exploring with
Axe re-equipped; these test gear changes were not manually saved.

## Native spellbook foundation

Exploration's former keyboard button now opens Spellbook; non-exploration
fallbacks still expose Keyboard. Spell browsing uses named, paged choices with
mixture counts and MP costs. Details show engine reagent requirements and
prerequisite errors. Cast is offered only when the engine prerequisites pass;
actual casting remains in gameCastSpell/spellCast. Native party selection now
also serves gameGetPlayer; moon-phase and energy-field choices are native.
Direction targeting continues to use the existing directional controller.

Simulator build passed. Runtime/visual checks: Spellbook shows Lindsay's 0 MP
and zero mixtures; Cure details show 5 MP, Ginseng/Garlic, and None Mixed with
no Cast action. Simulator currently shows Cure details in portrait.

Successful casting, MP/mixture deductions, all target types, multiple party
members, and combat must still be tested. A suitable simulator-only caster
fixture is needed; the bundled starting character is a Fighter. Mixing still
uses the legacy flow and needs native reagent/quantity selection. Canceling the
native energy-field picker now exits before the legacy invalid-key MP penalty;
this is an intentional mobile cancellation behavior. Browsing currently enters
the original Cast command, whose turn accounting still applies on exit.
Physical phone installation remains unchanged.

## Native recipe mixing

Spell details now offer Mix when the exploration controller is active and the
required reagents are available. Quantity presets are 1, 5 when possible, and
the computed maximum. No ingredients are reserved until quantity confirmation.
The selected batch uses Ingredients/addReagent and gameSpellMixHowMany, so the
engine owns reagent deductions, recipe validation, and mixture increments.
A shared availability helper caps at 99 mixtures and clamps invalid negative
stock to unavailable. Boundary/no-mutation tests and simulator build passed.

Runtime verified using the bundled Fighter fixture: Cure initially has 0 mixed
and capacity 3; Mix 3 succeeds; spellbook shows Cure — 3 mixed; details then show
capacity 0, no Mix action, and Not Enough MP (instead of None Mixed). This verifies
the UI/engine mixing path, but exact individual stock values have not yet been
inspected after saving. The Fighter has no MP, so successful casting is still
pending a simulator-only caster fixture. Current simulator shows Cure details
with three unsaved mixtures. Physical phone unchanged.

Pending mixing improvements: arbitrary quantity picker, reagent stock breakdown,
save/reload resource verification, and clearer turn accounting on spellbook exit.

## Native pause and manual save

Exploration now replaces the Escape button with Menu, offering Save adventure,
Touch controls, and Resume. Saving follows the engine's existing save eligibility
and also persists the topic journal; the result reports failure if either fails.
This is manual saving, not automatic interruption recovery. The inherited save
writer is not yet transactional.

The simulator build succeeded and was installed/launched. Runtime tapping and
save/reload verification remain pending: computer-use reported that the Mac was
locked. The prior three-mixture experiment was unsaved and was discarded when
restarting the simulator for this build. The physical phone remains unchanged.

## Spellbook cancellation and turns

The shared cast flow now reports whether a cast was attempted or a mixture was
made. iOS exploration and combat dispatchers use that result for turn completion:
browsing or cancelling caster/spell/target selection costs no turn, while an
attempted cast (including engine failure) or confirmed mix retains its turn cost.
Desktop dispatchers retain their prior behavior. This intentionally replaces the
legacy charge-on-opening behavior for mobile panels. Simulator compilation and
diff whitespace checks pass; runtime move-counter and combat checks remain
pending. This latest change is built but not installed on the physical phone or
simulator yet.

## Native direction prompts

The shared direction prompt now offers named North/South/West/East actions and
Cancel on iOS. Talk, open, unlock, attack, cannon, destroy, and spell flows provide
an action-specific prompt. Directions still feed the existing engine range,
obstruction, and effect rules. Desktop input remains unchanged. Simulator build
and whitespace checks passed. This is an explicit direction chooser, not entity
snapping or target cycling; those richer targeting affordances remain pending.
Runtime verification and physical-phone installation remain pending.

## Recipe stock visibility

Spell details now list current mixture count and each required ingredient with
its per-mixture requirement, available stock, and a textual missing indicator.
This makes mixing limits understandable without another inventory screen.
The from-direction spell prompt also distinguishes its source direction from
ordinary cast direction. Simulator build and whitespace checks pass; visual
verification remains pending because the Mac is still locked.

## Readable companion conditions

Party lists, companion details, and spell recipient selection now expose the
engine condition as Healthy, Poisoned, Asleep, or Dead. Recipient rows also show
maximum HP so healing choices have context. This uses the engine status rather
than inferring condition from health, and preserves disabled-target eligibility.
Simulator build and whitespace checks pass. Condition fixtures and larger-party
layout still require runtime verification.

## Exploration and transport command access

Menu now includes Explore and interact (search, open/unlock door, chest, camp,
dungeon torch, available gems) and Travel and transport (ascend/descend,
board/leave, ship cannon, horse pace, available sextant). Context limits resource
and transport actions; the existing game dispatcher enforces final legality and
turn cost. Merely browsing these groups does not dispatch a command. This adds
touch entry points, not a claim that all downstream flows are native: camp,
gem viewing, and some dungeon sequences still need review. Frequent interaction
commands should eventually move to the main action cluster per the playbook;
these nested groups are provisional access for the prototype.
Simulator build and whitespace checks passed. Runtime verification pending.

## Owned quest-item picker

Explore and interact now offers Use a quest item. The picker queries existing
item ownership predicates and use handlers, showing owned items only, omitting
duplicate aliases and a destroyed skull. Selection invokes the existing itemUse
rules; cancellation consumes no turn. This replaces the initial typed item name;
stone-puzzle follow-up prompts still need native adaptation and endgame tests.
Simulator build and whitespace checks passed. Runtime fixtures remain pending.

## Stone offering input

Altar-room and Abyss stone-name prompts now show owned stone colors as touch
choices, including all owned colors rather than revealing the correct answer.
Cancel clears the pending offering count and selection mask. Existing engine
checks still decide puzzle success and progression. The synchronous input now
uses an owning std::string through itemHandleStones, avoiding the old temporary
c_str lifetime bug on these paths. Desktop still accepts typed stone names.
Simulator build and whitespace checks pass. Full altar-room/Abyss fixture runs
and terminal Codex input adaptation remain pending; endgame completion is not
yet verified.

## Virtue question input

Shrine meditation subject and Abyss altar virtue questions now offer all eight
virtues on iOS, preserving the question and requiring the player to choose;
no answer is highlighted or filtered. Desktop retains typed input. The Abyss
path also replaces the dangling gameGetInputC result with an owning string.
Simulator build and whitespace checks pass. Shrine cycle/mantra entry and Codex
questions are still pending, as are runtime progression tests.

## Meditation touch inputs

Meditation duration now offers 1–3 cycles and Leave shrine. Mantra selection
suggests exact previously-exposed words from the eight-mantra catalogue, never
inserting that catalogue into the exposure journal or filtering to the current
shrine's correct answer. Custom entry remains available. Explicitly ending
meditation ejects without treating cancellation as a wrong mantra; this is an
intentional mobile cancellation adjustment. The catalogue matches current
conf/maps.xml and will need synchronization if custom shrine data is supported.
Simulator compilation and whitespace checks passed; shrine progression and
karma behavior still need runtime fixtures. Final Codex input remains pending.

## Exact puzzle discovery regression coverage

Mantra suggestions now use a dedicated whole-word exposure query, separate from
U4 dialogue's four-letter prefix matching. Regression tests verify that Summer
does not teach SUMM, rumors does not teach MU or RA, querying never teaches an
answer, capitalization is normalized, and exposure persists across reload but
clears for a new journal. Portable tests pass with warnings treated as errors.
This checks discovery behavior, not the full shrine runtime sequence.

## Codex virtue and principle questions

The Codex question sequence now displays its actual question with all eight
virtues and all three principles as touch answers. It never narrows choices to
the expected answer or changes the existing retry checks. This assessment picker
requires an answer; it does not expose the shrine's Cancel affordance. Desktop
continues to use typed responses. Simulator build and whitespace checks pass.
Word of Passage, final encompassing-answer input, presentation, and end-to-end
Codex runtime verification remain pending.

## Remaining Codex text answers

Word of Passage and final encompassing-answer prompts now use a native picker
of previously encountered conversation words, plus custom entry. Suggestions
are deduplicated, paged eight at a time, and never sourced from solution data.
A composed answer not encountered as a complete word still needs custom entry.
This broad word list is provisional: it needs topic curation/search for a long
adventure and has not been validated for endgame usability. Existing answer and
retry checks remain unchanged. Simulator build and whitespace checks pass.

## Native-panel simulation pause correction

Inspection found that pushing a modal controller leaves the GameController timer
registered. It continued wind/moon updates and balloon movement, and elapsed
reading time could cause immediate auto-pass after closing. Native panel reads
now hold a scoped pause depth; GameController skips simulation timer updates
while that depth is nonzero. Closing the last native panel resets idle command
time. This covers normal and immediate callback paths and is intentionally a
mobile pause policy. Simulator build and whitespace checks pass. Long-read,
balloon, nested-panel and background/resume runtime tests remain pending; this
is not yet full application-background or autosave support.

## SDL mobile lifecycle events

Both the main SDL loop and timed-presentation loop now recognize iOS background
and foreground events. While backgrounded they discard gameplay key events and
normal timer ticks; foreground completion resets idle command time so suspension
does not trigger an immediate auto-pass. Timed-presentation completion events
remain processed to avoid leaving a sleep loop stuck. Simulator build and
whitespace checks pass. Actual OS interruption testing remains pending; this
change does not save world state against process termination and is not an
autosave implementation.

## Save error detection

The engine save routine now checks every successful-path fclose result and each
dungeon tile fputc. Buffered write failures therefore return failure rather than
allowing the native menu to announce success. Simulator build and whitespace
checks pass. This improves error reporting only: the existing multi-file save
still overwrites in place, so transactional snapshots/recovery are required
before autosave can be considered robust. Disk-failure injection remains pending.

## Save snapshot foundation

Added a portable immutable-generation helper with an atomically replaced CURRENT
pointer. Publication requires all caller-declared files to exist and be nonempty,
flushes file/directory data before switching the pointer, and rejects unsafe
pointer names. Tests prove an incomplete replacement leaves the prior snapshot
selected, a complete replacement switches generations, and malformed pointers
are rejected. Tests pass with warnings treated as errors.

This helper is NOT YET integrated into gameSave or startup. Current gameplay
still uses the legacy multi-file paths. Integration must include dungeon and
outer-monster files, journal handling, new-game reset, initial legacy migration,
recovery errors, and bounded retention. File existence checks are not semantic
save validation. Process-kill and physical-device durability tests remain pending.

## Save reader audit and validation

Journey Onward now initializes its temporary save record and requires successful
parsing plus a party size of 1–8. Game initialization reads into a temporary
record and rejects incomplete/invalid-party records before installing them in
live state. The latter retains the engine's fatal-error behavior; a friendly
recovery screen is still needed. Simulator build and whitespace checks pass.

Snapshot integration remains pending. The reader audit also found dngmap.sav
writes but no literal dngmap.sav read reference in src. Dungeon restoration must
be traced and verified before claiming complete snapshot coverage.

## Dungeon save restoration

Game initialization now reads dngmap.sav for a saved dungeon, validates its exact
terrain byte count, and restores tiles and subtype bytes before creature loading.
Saving now includes permanent nonvisual annotations (such as an opened Abyss
ladder) and preserves base subtype bytes for unchanged terrain. Temporary and
visual annotations are excluded. Missing legacy dungeon files retain original
terrain; malformed present files use the engine fatal-error path pending recovery
UI. Simulator build and whitespace checks pass. This is a behavioral save-format
change requiring dungeon/fountain/room/monster and altar-ladder round-trip tests
before installation or release; those runtime tests remain pending.

## Snapshot gameplay integration

On iOS, manual saves and new-game creation now write into a fresh snapshot
generation. Publication requires party/monster files and, for dungeons, terrain
and outside-monster files. Startup and Journey Onward resolve the same selected
generation. Before the first snapshot, loading falls back to legacy files; an
existing invalid CURRENT pointer fails explicitly instead of silently selecting
an older legacy adventure. New-game writes now check serialization and close
results before publication. Desktop retains its original directory convention.

The discovery journal remains independently persisted in the settings directory,
so reading discoveries may survive an unsaved gameplay session. New-game creation
still clears that journal after publication; atomic adventure/journal reset is
not yet solved. Snapshot retention/cleanup, friendly recovery, semantic snapshot
validation, and process-kill tests are also pending. Simulator build and snapshot
unit tests passed. No autosave trigger or physical-phone installation yet.

## Adventure/journal snapshot consistency

Every published iOS generation now requires topics.txt. Manual saving serializes
the current journal into the staged generation before publication. New-game
creation stages an empty journal with its new party, so the CURRENT switch
selects both together and leaves the old adventure intact on pre-publication
failure. Journal loading and observed-dialogue writes resolve the selected save
directory (legacy root before migration). The live journal remains atomically
replaceable within the selected generation so discoveries survive without a
manual world save; world files remain immutable after publication.
Simulator compilation and whitespace checks pass. Process-stop/reset runtime
tests and snapshot retention remain pending. Earlier notes about root-level
journal reset after publication are superseded.

## Adventure/journal publication regression test

Added an integration test of TopicJournal and SaveSnapshot: publication fails
without the required journal, selection stays on the old adventure with its
known mantra, publication succeeds with an empty new journal, and the prior
generation retains its discoveries. The test passes with warnings as errors.
Party/creature files in this test are placeholders; this establishes journal
isolation/publication behavior, not engine serialization or runtime load success.

## Real serialization snapshot regression

The adventure snapshot test now links the actual savegame.c/io.c serializers
instead of writing placeholder party/creature files. It verifies party count,
move count, HP, weapon/armour, reagent stock, mixed spells, and creature tile and
coordinates after resolving and loading the selected generation. Old-generation
selection on missing journal and new empty-journal publication remain covered.
The compiled test passes. This proves serialization plus snapshot selection;
it does not yet exercise GameController, dungeon terrain, or UIKit save actions.

## Repeatable regression command

Added tests/run-mobile-tests.sh and README-mobile.md. The script builds its own
temporary binaries, uses the real C save serializers, runs all four current
mobile regression suites, and cleans up. All four suites passed together.
Interactive verification is still blocked by the locked Mac, reconfirmed through
Computer Use. No phone update or overall completion claim.

## Unlocked simulator: manual save/relaunch verified

Installed the current simulator build, opened Menu > Save adventure, and observed
success. Changed Lindsay from Axe to Hands through the native Party panel, saved
again, and inspected CURRENT selecting generation-IhK7DV with party.sav (502
bytes), monsters.sav and topics.txt. The saved record had moves=2 and weapon=0.
Terminated and relaunched with --skip-intro; the native companion detail confirmed
Weapon: Hands, Armour: Leather, HP 300/300. This establishes an actual UIKit →
engine save → snapshot publication → app relaunch → equipment display path.

Portrait companion text and buttons were visually readable with no overlap.
Legacy framebuffer/controls remain visible around panel edges and need polish.
Simulator is left in the paused companion panel. Physical phone unchanged.

## Full-window panel backdrop verified

Native panels now fill the window; their scrollable contents use safe-area
margins and the keyboard guide. Installed the build and visually inspected the
companion detail in portrait and landscape. No legacy controls/framebuffer show
through around the panel edges; text and equipment controls remain readable,
and rotation retains the selected companion and equipment state. Landscape
currently keeps the vertical content stack, placing bottom navigation below the
visible area; a landscape-specific composition is still required for polish.
Simulator is left on the companion detail in landscape. Physical phone unchanged.

## Separate landscape panel composition

The native panel's content stack now changes to equal-width detail/action columns
in landscape widths >=600pt, with actions aligned to the top. Portrait retains
its vertical detail-then-actions arrangement. Installed simulator build and
visually verified character details: all four equipment/navigation buttons are
visible in landscape, and upright portrait reflow preserves character state and
readable spacing. Upside-down portrait is unsupported by the app orientation
policy; rotating through it temporarily retains a landscape orientation.
Long conversations, large Dynamic Type, and keyboard-visible layouts still need
validation. Simulator is left paused in upright portrait character details.

## UIKit mixing/save/relaunch verified

Through native Spellbook > Cure > Mix this spell > Mix 3, the engine reported
success. Saved via Menu > Save adventure, terminated the app, and relaunched.
Spellbook showed Cure — 3 mixed. Recipe details confirmed Ginseng 0 (missing),
Garlic 1, capacity 0, and Not Enough MP for the Fighter. Starting stock was
Ginseng 3/Garlic 4 and Cure 0. This verifies exact deductions and mixture
persistence through the actual engine/UI save and startup path. Portrait recipe
text and Back control were visually readable. Successful casting still needs a
caster fixture. Simulator is paused on Cure details; physical phone unchanged.

## Successful native casting verified

Created simulator-only mage-fixture-tvh4w3h3 from the saved Fighter generation;
prior generation-EKMslz remains intact (pointer recorded in /private/tmp/ultima4-
pre-mage-snapshot.txt). Fixture is Test Mage, poisoned, 20 MP, 3 Cure mixtures,
with Hands/Skin appropriate for the class. Native Spellbook > Cure > Cast spell
completed without typing. Party subsequently displayed Healthy and 16/38 MP;
spellbook displayed 2 Cure mixtures. The 16 MP result matches 5 MP spent plus
one turn's regeneration. Simulator remains paused in the Mage spellbook, with
that cast unsaved. This verifies the single-member Cure path, not multi-member
selection, direction spells, combat casting, or failures. Phone unchanged.

## Spellbook cancellation and large text

Observed 16 MP and two Cure mixtures before closing/reopening the spellbook;
values remained unchanged, with no immediate idle pass observed. This is resource
and UI evidence, not a direct move-counter assertion.

Tested the largest Dynamic Type category. Existing two-column spell buttons
wrapped into narrow fragments; fixed native panels to use full-width action rows
and vertical composition for accessibility categories. Installed build and
visually verified readable spell names/costs at accessibility-extra-extra-extra-
large. Restored simulator content_size to its original large setting. Restarting
for the build restored the saved poisoned Mage fixture (20 MP, three mixtures),
so the earlier successful cast remains an unsaved test. Physical phone unchanged.

## Pre-publication parsing and engine reader corrections

Snapshot publication now reparses party/creature files, rejects trailing bytes
and invalid party counts, and parses the journal before switching CURRENT.
Tests exposed io.c readChar returning success at EOF; corrected it to reject EOF
without assigning a bogus byte. Also fixed the monster reader's final field to
read unused2 instead of overwriting unused1. Tests now verify both independent
field values and rejection of truncated party/creature and overlong party files.
All four mobile suites and simulator build pass. This validates structure, not
all semantic coordinates/map identities; dungeon validation/recovery still needs
further work. Latest build is not installed yet.

## Touch vocabulary cleanup

The iOS exploration startup hint now points to Menu instead of Alt-h. The Space
button is labelled Wait, with accessibility label Wait one turn, during native
exploration; legacy contexts retain their existing key control. Native spell
selection no longer emits redundant desktop Cast/Player/Spell input prompts.
Simulator build and whitespace checks pass. Latest changes not installed yet.

## Two-member caster fixture prepared; interaction blocked

Installed the latest save-validation/label build and created independent simulator
snapshot party-fixture-39wen5fd: healthy Test Mage and poisoned Test Companion,
with two members and three Cure mixtures. Prior snapshots remain unchanged.
The app launched, but Computer Use reported the Mac locked before interaction.
Terminated the simulator app to prevent unattended idle turns. Multi-member
caster/recipient validation is not yet performed. Phone unchanged.

## Caster/recipient prompt clarity

The shared party picker now accepts an action-specific prompt. Spellcasting uses
Choose who will cast the spell, then a recipient prompt naming the selected
spell. Existing member eligibility and active-player behavior remain intact.
Simulator build and whitespace checks pass. The two-member fixture remains
ready for interaction once the Mac is unlocked; this build is not installed yet.

## Creature startup validation

Both current-map and outside-map creature reads now parse into temporary tables,
check read success and exact EOF, and copy into live map tables only afterward.
Previously these callers ignored parser failure. Invalid present files currently
use the engine fatal-error path; friendly recovery is still a release blocker.
Simulator build and whitespace checks pass. Missing legacy creature files retain
existing optional-file behavior. Latest build not installed.

## Periodic checkpoint trigger

Added an iOS checkpoint attempt after ten completed exploration turns, only when
GameController is active, no native panel is open, normal view is active, the
party is alive, and the existing context permits saving. The call is after turn
housekeeping; combat/death early returns do not checkpoint mid-transition.
Successful manual saves reset the interval; failures report visibly and retry
after another ten turns. Uses the validated snapshot writer. Simulator build
and whitespace checks pass; this latest build is not installed or runtime-tested.

This is not a full interruption guarantee: towns/combat still follow original
save restrictions, and transient state restoration, process-kill testing,
snapshot retention, and immediate background checkpoints remain pending. Avoid
claiming production-safe autosave until those gates are satisfied.

## Conservative snapshot retention

Successful publication now retains current, immediate predecessor, and one other
recent recognized generation. Cleanup considers only generation-* directories
containing known regular save files; unknown files, symlinks, and separately
named test fixtures are left alone. It runs only after publication succeeds.
Tests verify both current/predecessor survival, the three-generation bound for
recognized saves, and preservation of unfamiliar content and symlinks. All four
mobile suites and simulator compilation pass. Latest build not installed;
periodic-checkpoint gameplay testing remains pending.

## Periodic checkpoint verified through gameplay

Installed current build and advanced the two-member fixture using the native
Wait control. The engine displayed Adventure autosaved without a manual save.
Paused immediately and inspected CURRENT: generation-AsmLL9 contains party,
monsters and journal, with moves=13 versus fixture moves=3 and members=2.
Thus the first checkpoint occurred after exactly ten completed turns (one idle
pass was included before the explicit Wait sequence). Retention left current
generation-AsmLL9 and generation-EKMslz, while preserving separately named
fixtures. Simulator remains paused in Menu. Dungeon and failure/recovery cases
are not established by this world-map test. Physical phone unchanged.

## Two-member caster/recipient runtime verified

Opened Spellbook from the active two-member adventure. Native caster picker
showed Test Mage Healthy and Test Companion Poisoned (280/300 HP). Selected Mage
(30 MP), Cure, Cast spell; recipient picker explicitly named Cure and displayed
both characters and conditions. Selecting Companion changed that member to
Healthy while retaining 280/300 HP. Mage details showed 26/38 MP, matching Cure's
5 MP cost plus one turn's regeneration. This verifies distinct caster/recipient
routing through actual UIKit and engine execution. Simulator remains paused in
Mage details with this cast not manually saved. Combat and cancellation during
recipient selection remain separate pending cases. Phone unchanged.

## Direct combat action entry

When a CombatController owns input, the touch Talk control becomes Attack and
the keyboard slot becomes Spells. Both dispatch directly to the active engine
controller; combat attack direction now uses the native cardinal chooser.
Exploration-only Party/Enter/Journal controls are visibly disabled during combat.
The battlefield/HUD still uses the legacy rendering fallback, distance-selecting
weapons still need a native range picker, and cancelling an attack retains the
legacy turn cost. Simulator compilation passed; live combat tests and a complete
combat-specific HUD/action layout remain pending. Latest build not installed.

### Combat range and cancellation

Combat attack now returns whether an attack was committed. On iOS, cancelling direction or distance keeps the current character's turn. Weapons with selectable distance use native 1–9 tile choices bounded by the weapon range, with an explicit Cancel attack action. Confirmed attacks retain the engine's miss, projectile, and weapon-loss behavior. Desktop turn dispatch remains unchanged.

Simulator build succeeded; all four portable regression suites passed. These suites do not exercise combat turn dispatch; encounter-level runtime verification remains required. Mac access restored and simulator deployment resumed. No redesigned physical-phone installation yet.

### Native battle pause menu

The combat HUD now offers Menu and Wait instead of Esc and Spc. Menu directly opens a paused native panel with the current actor, each party member's health, magic and condition, battle instructions, and Resume. It does not send a combat command or finish a turn. Combat Wait accessibility explicitly says it passes this character's turn. The menu intentionally omits saving because the existing save format does not restore an encounter. Source review confirmed legacy combat Esc was a bad command outside debug mode.

The simulator build passed. Encounter-level pause/resume and cancellation tests remain outstanding; compilation and source review are not runtime proof. Combat still uses the legacy battlefield/status rendering and needs its dedicated mobile composition.

### Battle HUD and encounter evidence

Combat now uses the enlarged 176×176 battlefield viewport and native two-line actor status (name, condition, HP, MP, weapon), with the native recent-message log. The simulator encounter fixture `combat-fixture-fjcf6rw2` copies the previous save and adds a rat adjacent to the avatar; the original `generation-AsmLL9` is retained and its name is recorded in `/private/tmp/ultima4-pre-combat-snapshot.txt`.

Runtime evidence: pressing Wait entered combat, native status identified Test Mage with HP300/300 and Magic31, and Attack opened cardinal-direction buttons with Cancel. The initial cancellation test was inconclusive because the active actor changed across observations. Investigation confirmed the desktop 20-second idle auto-pass remained enabled. On iOS it is now intentionally disabled: reading time must not spend turns; explicit actions and Wait advance turns. This is a deliberate timing deviation from desktop, following the playbook's interruption and touch-reading guidance. Wind/moon animation and airborne transport behavior are otherwise unchanged. Controlled cancellation and visual orientation checks remain required after installing this final build.

### Controlled combat runtime checks

Verified on the installed simulator build with mobile idle pass disabled, using the isolated rat encounter fixture:

- Attack → Cancel retained Test Mage as active actor, HP300/300 and Magic31 unchanged.
- Menu displayed both party members, current actor, health/magic/conditions; Resume retained the same actor and resources.
- Actor remained Test Mage throughout multiple observations exceeding the old 20-second idle threshold.
- Explicit Wait advanced to Test Companion, Poisoned, HP278/300, Magic9.
- Visually inspected portrait and landscape screenshots: square battlefield, actor status, recent log and touch controls were visible without overlap. Rotation retained the actor and resources.
- Left simulator paused in the battle menu in landscape.

These checks establish direction-cancel, pause/resume, explicit-pass and orientation behavior for this encounter only. Selectable weapon distance, spell cancellation during combat, winning/fleeing, dungeon combat, larger text and the broader end-to-end game remain unverified. Disabled Party/Journal/Enter buttons and the legacy Return button visibly clutter the battle action area; replace with useful combat actions during further polish.

### Battle party and weapon management

Party is now enabled during combat. The existing native party panel permits inspection of every member, while only the current combat actor receives Change weapon. The button and party introduction state the turn cost. Class restrictions, spare stock filtering and the engine's setWeapon implementation remain shared with exploration. Successful changes complete one turn through the owning controller; browsing/cancellation does not call finishTurn. Armour changes are omitted during combat, matching the original command restrictions.

Simulator build succeeded (`/private/tmp/ultima4-battle-equipment-build.log`). This build has not yet been installed or runtime-tested; the running simulator remains paused on the previous build. Next check: active versus inactive member actions, weapon cancellation, successful equip advancing exactly one actor, and updated inventory stock.

### Combat equipment runtime inspection

Installed the combat equipment build and entered the rat fixture. Party is usable during battle. Inactive Test Companion details offered only Back/Close, with neither weapon nor armour changes. Active Test Mage details offered Change weapon — uses turn and no armour change. Its weapon list had no class-legal spare item; Cancel returned to unchanged Mage details (HP300/300, Magic31, Hands/Skin). Successful equip turn advancement and stock accounting still need runtime verification.

The empty list lacked an explanation. Added explicit no-spare-equipment wording shared by weapon/armour pickers, retaining Cancel and current equipment. Simulator remains paused in the Mage detail panel on the preceding build; the message change is not yet installed.

### Successful combat equipment runtime check

On the installed combat-equipment build, closing the Mage's panel retained the Mage's turn. Explicit Wait advanced to Test Companion. Companion's weapon picker offered Axe — 1 spare. Selecting it closed the panel, printed `Test Companion equipped Axe.`, and advanced to Test Mage (Magic31→32 at the round boundary). A subsequent explicit Wait returned to Test Companion, whose native HUD and engine log both showed Axe. This verifies a successful combat weapon change and expected actor advancement in the two-member fixture. Simulator left paused in the battle menu. Spare-stock conservation after unequipping and selectable-distance weapons still require runtime checks; the menu intentionally excludes the currently equipped item, so absence from that list alone cannot prove stock conservation.

### Combat spell recipient cancellation

Runtime-verified on active Test Companion's turn: native Spellbook identified the correct caster with 9MP; Cure showed 3 mixed spells and 5MP cost. Cast opened native party recipient choices, including poisoned companion. Cancelling there returned to the same companion's turn, Poisoned HP278/300, Magic9, Axe. Reopening Spellbook confirmed Cure still had 3 mixtures. Thus cancellation at recipient selection spent neither turn, MP nor mixture in this encounter. The simulator is paused in the spellbook. The legacy log still says `Cast Spell! Cure! Who: None` on cancellation; replace that wording during feedback polish.

### Successful native combat Cure

Verified actual Cure cast by Test Companion targeting self: Poisoned→Healthy, HP remained278/300, MP9→5 (5MP cost plus normal round-boundary regeneration), active actor advanced to Test Mage. Reopened the spellbook and observed Cure mixtures3→2. This establishes effect, cost, mixture consumption and actor progression for a targeted combat spell in this fixture. Simulator is paused in Test Mage's spellbook, with the successful test state unsaved. Other spell parameter types and end-to-end encounters remain to be verified.

### Touch quest-item use in combat

Extracted the exploration owned-item chooser into shared `gameUseQuestItem()`. Combat Use now opens that native chooser, restores the party stats view, and preserves the turn on top-level cancellation. Battle Menu exposes Use a quest item, dispatching through the existing combat controller. Owned-item filtering and engine item effects remain shared with exploration. Cancelling deeper item-specific puzzles may still count as an attempted use under existing behavior; this change does not claim those flows are fully cancellation-safe. Runtime item-effect testing remains required. Current simulator remains paused on the previous build.

### Contextual dungeon-combat chest command

Combat command audit found Get Chest had no native entry point. Battle Menu now offers Open chest — uses turn only when the active position contains a chest tile or chest object, using the same detection criteria as getChest(). It dispatches combat 'g', retaining active-member trap resolution, loot, and turn behavior. This closes a touch command gap for dungeon rooms. Build verification is in `/private/tmp/ultima4-combat-chest-build.log`; runtime dungeon chest checks remain pending, and this build is not installed yet.

### Vendor numeric and party input groundwork

Vendor INPUT_NUMBER now calls shared gameGetAmountInput: iOS uses +/- decimal increments, explicit Confirm amount and Cancel (zero), bounded by script digit count up to six digits. It permits arbitrary integer amounts without a software keyboard; scripts retain quantity/payment validation and effects. Desktop retains ReadIntController. Vendor INPUT_PLAYER uses native party selection on iOS, including disabled members for healing services.

This is groundwork, not a completed shop redesign: product/yes-no choices, continuations and free strings still use legacy input, and the amount panel needs the preceding vendor text plus semantic quantity/payment titles. Runtime purchasing/healing and cancellation behavior require validation. No physical-phone update.

### Vendor quantity context

Script now retains up to 8192 bytes of text actually emitted to the screen between input steps. Person clears this buffer at vendor introduction and after each accepted input before continuing. The native amount panel displays this text above its amount controls, keeping the current vendor question/price visible during selection; internal script output buffers are excluded. Runtime verification remains needed for inventories generated through nested script actions and for long shop text. The build log is `/private/tmp/ultima4-vendor-context-build.log`; simulator remains on the earlier combat-equipment build.

### Native vendor choice and continuation panels

Vendor INPUT_CHOICE now uses a native panel containing the current emitted script dialogue and allowed options. Yes/No, Buy/Sell and Food/Ale have semantic labels; remaining product choices currently retain their letter labels beside the script's displayed inventory. Cancel maps to the existing unset-variable branch. INPUT_KEYPRESS uses a visible Continue button. Desktop input remains unchanged through helper fallbacks. Build log: `/private/tmp/ultima4-vendor-choice-build.log`.

This removes more software-keyboard dependence but is not the finished shop interface: product names should replace letter buttons, dynamic inventory rendering needs runtime validation, and tavern text inputs still need topic/custom-entry handling. End-to-end purchase, sale, payment, healing and exit tests remain pending.

### Vendor product names

Vendor choice buttons now resolve labels from the script's item noun and choice identifier, using the existing scoped lookup (local vendor then parent catalogue). Buy/Sell, Yes/No and Food/Ale retain semantic action labels. Only already-allowed script choices are rendered; name lookup does not expand the choice set. Missing metadata retains the letter fallback. Compile verification is recorded in `/private/tmp/ultima4-vendor-names-build.log`. Product labels, local overrides and sale catalogues still need runtime shop testing before this is considered complete.

### Vendor dialogue discovery

Vendor script text actually emitted before each input and its final reply now enter the same persistent conversation journal as ordinary NPC dialogue. This enables discovered shop/tavern clues to inform keyword exposure without scanning hidden script responses. Empty segments are skipped and existing journal deduplication applies. Script audit found the only INPUT_STRING in vendorScript.xml is the tavern topic question; that still requires a dedicated discovered-topic chooser with custom entry. Build log `/private/tmp/ultima4-vendor-journal-build.log`; runtime shop discovery and persistence remain unverified.

### Exposure-gated tavern topics

Tavern text input now uses a native topic panel plus optional custom entry and Never mind. Script supplies candidate identifiers from the target branch; the UI filters them against actually displayed journal passages. Multiword topics require contiguous normalized whole-word exposure within a single passage, avoiding accidental discovery from unrelated words in separate dialogue. No hidden response content is observed. Added regression assertions for case/newline handling, word boundaries, unseen topics and unrelated separate passages. The simulator build passed; runtime tavern payments, candidate resolution and custom entry remain pending. The currently running simulator has not been updated to this build.

### Shop runtime fixture ready

Installed the compiled tavern/shop build, then created separate simulator snapshot `shop-fixture-y8g4purl` from the prior fixture. Set location to Jhelom (map7), position23,10 (brick floor immediately west of weapon vendor24,10, person29), preserving world return coordinates. Monster table cleared in this fixture. Previous CURRENT name recorded in `/private/tmp/ultima4-pre-shop-snapshot.txt`; original snapshots remain intact. App launched successfully with --skip-intro. CUA then reported the Mac is locked, so no shop interaction was performed. Next runtime action: Talk East, Buy, select an affordable weapon, validate price/quantity/transaction and cancellation. Do not claim shop transaction validation from successful launch alone.

### Shop amount cancellation correction

Source audit found native amount Cancel was indistinguishable from Confirm0. In the tavern's ale branch, zero is interpreted as refusing payment. Native Cancel now returns a separate negative sentinel; Person terminates the vendor script before continuing the pending transaction. Confirm0 still explicitly submits zero under game rules. This supersedes the earlier note describing cancellation as zero. Already completed purchases/tips are not rolled back. Runtime cancellation checks remain necessary once simulator UI is accessible. Build log `/private/tmp/ultima4-vendor-cancel-build.log`.

### Touch beggar donations

GIVEBEGGAR now opens the native amount picker with the conversation text and carried gold. Confirmed amounts still flow through Person::beggarGetQuantityResponse and Party::donate, preserving affordability and virtue effects. Cancel maps to zero only for this specific handler: source inspection confirms zero returns to TALK without donating or modifying gold. Simulator build passed (`/private/tmp/ultima4-beggar-amount-build.log`). Runtime donation and virtue checks remain pending. Simulator access was rechecked this turn and still reported the Mac locked.

### Touch character-creation choices

Character sex selection now uses Male/Female buttons, with Cancel returning before starting the creation sequence. Virtue-question lead-ins have native Continue; each full question has Choose A/Choose B buttons driving the original doQuestion tree. Required questions omit Cancel rather than silently selecting B. Name entry and the other introductory story/ending continuations remain legacy and need a full onboarding pass. Build log `/private/tmp/ultima4-creation-choices-build.log`; runtime new-game validation is outstanding. The shared choice helper's vendor-oriented name is now broader than its uses and should be renamed during UI consolidation.

### Native opening story passages

The 24 opening passages now appear in native readable panels with passage count and Continue. Both post-creation transitions use native Continue as well. Original text/order and underlying scene/animation calls remain intact; the current opaque native panel covers illustrations while reading, so integrating story artwork into the native layout is still needed for polish. Desktop retains its original key-based flow. Build log `/private/tmp/ultima4-native-story-build.log`; new-adventure runtime and rotation tests remain outstanding.

### Native character name field

iOS creation now uses the native panel with Enter name, Character name field/accessibility label, Done return key, 12-character engine limit and Cancel creation. Existing ASCII letters/numbers/spaces input policy is disclosed in the prompt; blank-only names abort creation like empty input. Native input uses the existing keyboard avoidance layout. The desktop menu-area name reader remains unchanged. Build log `/private/tmp/ultima4-native-name-build.log`; real keyboard, safe-area, cancellation and full creation tests remain pending. Names matching internal cancellation values `cancel`/`bye` currently exit; eliminate this string-sentinel collision in a typed panel-result refactor before release.

### Distinguish submitted text from panel actions

Native panel callbacks now include whether the result came from text entry or an option button. Character-name acceptance uses this origin rather than treating names `cancel` or `bye` as control tokens. Tavern custom text also preserves those strings while Never mind remains cancellation. This supersedes the prior name-sentinel limitation. Existing callbacks were updated together; simulator build passed (`/private/tmp/ultima4-panel-input-kind-build.log`). Runtime keyboard and cancellation checks remain pending.

### Lord British confirmation touch path

Conversation::CONFIRMATION returns INPUT_CHARACTER, so it bypassed the existing native ASK/ASKYESNO handling. It now presents Yes/No with the accumulated displayed dialogue and passes the selected character into the unchanged response logic. Other legacy character-input states remain unchanged pending their reachability audit. Build log `/private/tmp/ultima4-lb-confirm-build.log`; runtime Lord British dialogue remains unverified. Mac lock was rechecked and still prevents simulator interaction.

### Native companion ordering

Exploration Party now offers Reorder companions for parties with at least three members. Two native selections exchange non-Avatar companions via Party::swapPlayers; the first selection is omitted from the second list. Avatar leadership is preserved, both cancellation paths leave order/turn unchanged, and a confirmed swap completes one exploration turn like the original command. The action is unavailable during combat. Build log `/private/tmp/ultima4-party-order-build.log`; runtime tests with three or more members remain required.

### Readable shrine advice and clue retention

Successful meditation now presents its result with native Continue. Non-elevation advice appears in a native text panel and is recorded only after showVision reveals that exact advice, making it available in the journal and exposure system. Unrevealed shrineAdvice entries are never observed. Elevation rune artwork retains its original visible-image continuation pending a dedicated image-aware panel. Build log `/private/tmp/ultima4-shrine-advice-build.log`; shrine runtime and save/reload checks remain pending. Mac lock was rechecked and still prevents simulator testing.

### Shrine journal source labels

Revealed-text recording now accepts an explicit source, preserving the same persistence/discovery behavior. Shrine advice is labeled `Vision at the Shrine of <virtue>` instead of a generic conversation, making later journal review attributable. Ordinary and vendor dialogue retain their prior location-based labels. Build log `/private/tmp/ultima4-journal-source-build.log`; runtime presentation remains unverified.

### Accumulated regression and iPhone target build

After the shop, onboarding, input-origin, party-order and shrine changes, all four portable suites passed again (topics/rules/snapshots/adventures). An unsigned Release arm64 iphoneos build of the physical-device target also succeeded, log `/private/tmp/ultima4-current-device-build.log`. This is compilation/link validation only; nothing was installed on the phone.

Save-loading audit confirms gameSaveDirectory still treats an existing invalid CURRENT pointer as fatal. Automatic fallback is not implemented; a recovery design must distinguish previously published generations from complete-but-unpublished staging directories before offering rollback. User-visible recovery remains a release requirement, along with the outstanding gameplay/runtime work.

### Durable previous-checkpoint reference

Save publication now atomically writes/fsyncs PREVIOUS to the old selected generation after validating/syncing the new files and before switching CURRENT. First publication leaves PREVIOUS absent; republishing the same generation does not overwrite it. Shared pointer parsing enforces the same name/path constraints. Regression tests verify PREVIOUS identifies the prior selected checkpoint and remains readable after deliberately corrupting CURRENT. All four portable suites pass. Simulator build log `/private/tmp/ultima4-previous-save-build.log`.

This is recovery groundwork only. The loader still needs validation and an explicit user-facing recovery choice; it does not automatically load PREVIOUS. A failed current-pointer publication may leave PREVIOUS equal to the still-current generation, which is safe but not an older rollback. Existing installations acquire PREVIOUS on their next successful checkpoint.

### Recovery pointer retention

Snapshot cleanup now protects the generation named by durable PREVIOUS independently of the caller's in-memory predecessor hint. This prevents cleanup after restart or without a hint from deleting the recovery reference's target. Updated retention regression runs cleanup with an empty hint and verifies the prior snapshot survives alongside current and one additional generation. All four portable suites passed. User-facing recovery and corrupted-file validation remain outstanding.

### Initial user-facing recovery flow

Journey Onward now preflights an invalid existing CURRENT pointer before gameSaveDirectory can terminate the process. When PREVIOUS resolves to a world save with structurally valid party/creature files and readable journal, a native panel offers Restore previous checkpoint or Back to menu. Restoring atomically replaces only CURRENT; save folders remain. If no validated world candidate exists, the player gets an explanation and returns to the menu. Write failure is also reported without continuing load.

This is intentionally incomplete recovery coverage: current-file corruption, missing CURRENT, dungeon terrain validation, --skip-intro bypass and broader semantic save validation remain outstanding. Runtime restore/decline/failure testing is required. Build log `/private/tmp/ultima4-recovery-menu-build.log`.

### Character enum validation in saves

SaveValidation now checks every active member's class, weapon and armour ranges, preventing malformed values from reaching indexed engine lookups. Applied to party-file validation, Journey Onward eligibility and game initialization. Added regression cases for invalid class/weapon/armour values; all four portable suites pass. Simulator build log `/private/tmp/ultima4-save-character-validation-build.log`. This is not full semantic validation: coordinates, names, counts and dungeon state still need review, and direct-load errors still need complete recovery coverage.

### Current snapshot preflight

Journey Onward now validates selected party/creature files and journal before proceeding. A failed check enters the existing previous-world-checkpoint recovery choice, retaining all snapshot folders. Missing CURRENT with PREVIOUS present also enters recovery instead of silently falling back to a legacy adventure. A first-run/legacy installation with neither pointer retains its old path. Dungeon terrain/outer-monster checks and direct --skip-intro loading remain outside this preflight and require further work. Simulator build log `/private/tmp/ultima4-current-save-preflight-build.log`. Runtime recovery testing remains blocked by the Mac lock, reverified this turn.

### Dungeon checkpoint recovery validation

Shared checkpoint preflight now checks the original U4 dungeon IDs17–24, 8×8 position bounds, level0–7, structurally complete outer-world monster file and exactly512 terrain bytes. Journey Onward uses it for both current and previous candidates, allowing validated dungeon checkpoints to be offered for recovery. World checkpoints retain common party/monster checks. Added tests rejecting missing dungeon dependencies and511-byte terrain and accepting a complete512-byte fixture. Terrain token semantics, full monster coordinates and actual dungeon restoration still require runtime testing; this does not establish complete dungeon-save fidelity. Build log `/private/tmp/ultima4-dungeon-recovery-build.log`.

### Direct-launch preflight

iOS --skip-intro now runs the same checkpoint recovery preflight as Journey Onward and verifies a valid party file before entering game initialization. Declining recovery or lacking a playable party falls back to the normal intro/menu path. Build log `/private/tmp/ultima4-direct-launch-validation-build.log`; runtime recovery UI before an active intro controller still needs verification. The artificial shop fixture uses a city snapshot (not a normal world/dungeon checkpoint), so it will intentionally fail the stricter snapshot validator on this build; prepare a legacy-format city fixture or reach the shop from a valid world save for subsequent testing.

### Bounded saved names

Active party records now require a null terminator within the16-byte on-disk name field before they pass save validation. This prevents malformed imported/corrupted names from reaching C-string consumers beyond their buffer. Regression checks reject16 non-null bytes and accept15 characters plus terminator; all four portable suites passed. Full runtime recovery remains pending.

### Unlocked shop runtime check

Mac access restored. On the installed tavern/shop build (preceding later recovery/onboarding changes), Talk East from the Jhelom shop fixture opened Willard's greeting with native Buy/Sell. Buy displayed Axe, Sword, Crossbow and Halberd as named buttons with the script inventory text. Selecting Axe with200 gold produced the native insufficient-funds response; No exited and HUD still showed200 gold. A repeated Talk East returned no response; vendor movement metadata is128 (follows avatar), but the precise cause is not yet established. Do not claim repeated-conversation or successful transaction coverage. Simulator left paused in Menu, landscape. Next: identify vendor's current tile or reset fixture, then validate a sale and an affordable purchase.

### Shop retry and successful sale

Retrying Talk East in the existing runtime state reopened Willard's native Buy/Sell menu. Sell→Axe offered112gp; Yes completed the sale, and exiting showed gold200→312. This verifies a successful touch sale and proceeds. The sale list still offers every weapon type rather than only spare owned stock; stock filtering remains a UI improvement. The intermittent no-response cause remains unproven. Simulator paused in Menu with312 gold, enough to test buying back one225gp Axe; this state is unsaved.

### Latest simulator compilation and pending purchase verification

Release arm64 simulator compilation succeeded with the saved-name validation included (`/private/tmp/ultima4-name-validation-build.log`). This build was not installed, preserving the existing unsaved shop test. In that older running build, Buy→Axe→Yes displayed the successful purchase response, then No was selected to exit. The final HUD read was truncated, and subsequent access attempts report the Mac locked; the expected312→87 gold deduction and reacquired spare Axe remain unverified. Do not count the purchase as fully verified until both are inspected.

### Verified purchase and direct-launch recovery

After unlocking, the older shop build showed87 gold after the225gp Axe purchase from312 gold. Party→Test Companion→Change weapon showed `Axe — 1 spare`, verifying both currency deduction and acquired inventory. The companion remained equipped with Hands; inspection did not equip the purchase.

Terminated the old session after that check and backed up all saved app data to `/private/tmp/ultima4-pre-recovery-18kivcqj/xu4` (the unsaved transaction itself is runtime evidence, not a persisted save). Set PREVIOUS to existing world checkpoint generation-AsmLL9, retaining the intentionally unsupported city snapshot as CURRENT, and installed the latest compiled simulator build. Direct --skip-intro launch displayed the native recovery explanation with Restore previous checkpoint / Back to menu. Restore resumed Britannia with Test Mage HP300/300, Food300, Gold200. Persistent CURRENT now equals generation-AsmLL9, PREVIOUS remains the same, and the shop fixture folder remains. This verifies the positive recovery branch before intro/controller initialization; decline, missing candidate, write failure and actual corrupted-file variants still need runtime coverage. Simulator is left in Pause menu. Physical phone installation is unchanged.

### Redesign installed on physical iPhone

At the user’s request, built and development-signed the current source for iphoneos (log `/private/tmp/ultima4-phone-redesign-build.log`, BUILD SUCCEEDED). Backed up existing phone Documents via devicectl to `/private/tmp/ultima4-phone-before-redesign`. The paired iPhone 14 Pro accepted the configured preview bundle; devicectl confirmed installation and launch with --skip-intro. This supersedes earlier notes saying the phone retains upstream. Installation/launch are verified; actual phone gameplay awaits user feedback. No simulator fixture data was copied to the phone.

Continued simulator rotation testing: upright portrait displays the world above readable status/log and bottom touch controls. Rotating through upside-down portrait retains landscape because that orientation is not supported; rotating onward to upright portrait correctly recomposes the HUD. Pause panel retained its accessible actions during rotation and Resume returned to gameplay. Further lifecycle/background and portrait panel verification remains. Simulator left paused.

### World-visible sheets requested by user

User explicitly rejects full-screen menus and requires seeing the main game throughout interaction, overriding the previous full-screen deep-panel design. Shared native panels now use a clear input-blocking root with a rounded bottom sheet in portrait / right-side sheet in landscape. HUD rendering reserves the remaining region for the entire map, hides underlying command buttons, and preserves the last world status while a native input controller is active. Panels still pause engine turns and retain scrolling, Dynamic Type, explicit choices/cancellation, and optional custom text. Keyboard layout reduces/repositions the sheet on iOS15+; this and older-iOS keyboard behavior need verification.

Simulator Release build passed (`/private/tmp/ultima4-sheets-build.log`), installed and launched. Mac locked before visual inspection, so geometry, rotation, scrolling, keyboard and renderer continuity remain unverified; do not describe these as runtime-passed. The physical phone still has the preceding full-screen-panel build. Next action: unlock Mac and inspect portrait/landscape Party, Journal and conversation panels before replacing the phone build.

### Portrait packaging defect found and corrected

User reported physical phone would not enter portrait. Inspection of the signed device app Info.plist proved both orientation arrays still contained landscape only: the device CMake project had stale generated plist settings. Regenerated the existing device project with cmake, rebuilt/signed successfully (`/private/tmp/ultima4-portrait-sheets-device.log`), and asserted portrait is present in both packaged iPhone/iPad orientation arrays. This supersedes prior assumptions that source plist changes alone proved phone orientation support.

Unlocked simulator visual verification: Party sheet in upright portrait leaves the full square map above; rotating with the panel open rearranges it beside the full map in landscape, retaining party choices and status. This verifies the basic shared layout and rotation, not all long-content/custom-keyboard flows. Attempted corrected phone update, but devicectl failed to locate device; list devices confirms paired iPhone unavailable. Corrected signed build is ready, not installed. User must reconnect phone to proceed.

### Corrected portrait/sheets build installed on phone

User requested retry. devicectl confirmed installation of the configured preview bundle on the paired iPhone 14 Pro and successful launch with --skip-intro. The phone now has the regenerated portrait-capable device build with world-visible sheets. Actual physical-device rotation and usability remain for on-device verification; simulator basic portrait/landscape Party sheet rendering was verified earlier.

### User screenshots: targeting and legacy-screen leakage

User reports direction sheets should reuse D-pad, DOS chrome behind nested spell/journal panels, transition flicker, and inadequate multiple-member HUD. Shared cardinal direction prompts now expose existing D-pad and a compact prompt/Cancel card; D-pad buttons submit cardinal choices directly to the waiting native input, suppressing movement. Other command buttons remain hidden. Direction-mode root passes touches through outside its card.

Renderer previously cleared cached world status when the active controller changed and no sheet was present during a transition, leading subsequent sheets to display the full legacy framebuffer. Cache is now retained across controllers while a normal-map/combat adventure remains visible, and cleared outside those contexts. This targets supplied spell/journal and transition cases; it does not eliminate every remaining legacy screen (intro, dungeon/special views still need dedicated treatment).

Simulator build passed (`/private/tmp/ultima4-transition-dpad.log`), installed/launched. Mac locked before runtime interaction, so D-pad/cancel/rotation and nested-screen/flicker regression verification are pending. Phone remains prior portrait/sheets build. Multi-member HUD proposal: compact health/condition roster with active-turn highlight and tap-through details while retaining map; not implemented yet.

### D-pad/transition fixes installed on phone

At explicit user request, regenerated the device project, built/signed current source successfully (`/private/tmp/ultima4-dpad-phone-build.log`), then devicectl confirmed installation and launch on the paired iPhone 14 Pro. Phone now includes the D-pad direction mode and retained world-state rendering changes. Installation/launch verified; their gameplay behavior is still unverified.

### Fixed map geometry and source-grouped journal

User reports Save Adventure still flashes legacy UI, map resizing is unwelcome, and journal lacks utility. Map now uses the same reserved geometry regardless of panel visibility (portrait upper region / landscape left region); landscape D-pad moves into command region. Normal-map/mixtures status no longer requires GameController to be topmost, removing a controller-dependent legacy fallback rather than relying solely on cached status. This remains a proposed fix for reported flashes, not visually verified.

Journal now groups already-read passages by recorded source, with scrollable collected notes and paged source navigation. Standalone normalized Bye/Farewell/Your Interest/What else and empty passages are omitted from presentation without deleting archive records. Speaker attribution and semantic topic indexing remain incomplete; no unseen topics or answers are introduced.

Simulator and signed device builds passed (`/private/tmp/ultima4-stable-map-journal.log`, `/private/tmp/ultima4-stable-map-phone.log`). Simulator installed/launched, but Mac lock prevented runtime verification. Following user’s prior install preference, updated physical iPhone; devicectl confirmed installation. Map stability, Save Adventure flicker, journal scrolling and landscape control geometry still require runtime testing.

### Larger map and single landscape control layout

Added shared ios/mobile_layout.h geometry for renderer, sheets and controls. Portrait map now uses near-full safe width with a shorter sheet below; landscape map uses available height on left, with status and controls/sheets on right. Geometry remains unchanged when panels open. Removed competing render-time D-pad placement; layoutSubviews alone positions it in the landscape command region.

Unlocked simulator verification: landscape West D-pad tap produced West log and visible map movement. Opening Party left map size/position unchanged. Rotated open Party through to upright portrait and visually verified the larger map above the shorter readable sheet. Caught landscape status truncation and changed it to three lines with Food/Gold separate; this last text-only adjustment compiled in the signed device build but was not visually rechecked. Simulator log `/private/tmp/ultima4-larger-map.log`, signed device log `/private/tmp/ultima4-larger-map-phone.log`, both successful. devicectl confirmed installation on paired iPhone14Pro. Further multi-direction, combat targeting, long sheet scrolling and smaller-device testing remain.

### Left-hand landscape D-pad and hold movement

User prefers D-pad on left in landscape and continuous movement while held. Moved fixed landscape map to center, status into left column above D-pad, actions/sheets right. Shared map dimensions stay unchanged across panels. Visually verified landscape in simulator and West tap produced movement/terrain feedback.

Added exploration-only repeat after350ms at160ms intervals; release/cancel/drag exit invalidates timer, and ticks stop if button no longer highlighted, a panel/controller intervenes, or app is not active. Direction selection remains one-shot; combat excluded to avoid spending successive actor turns. Sustained touch, release timing and interruption still need runtime verification. Simulator and signed-device builds passed (`/private/tmp/ultima4-hold-movement.log`, `/private/tmp/ultima4-left-dpad-phone.log`); devicectl confirmed phone installation.

### Wider overlapping panels, compact details and animation-only menu ticks

User explicitly permits landscape panels overlapping map to avoid narrow unreadable columns. Landscape sheets now expand to about340pt and overlap the right edge of the fixed map; map size/position stays unchanged. Shared scroll subclass allows cancelling touches that begin on controls, enabling drag-to-scroll across buttons. Compact-details flag preserves deliberate line breaks with Dynamic Type subheadline; party details use five compact stat lines plus two action rows. Visually verified all four detail actions fit in both landscape and upright portrait on simulator at default text size. Accessibility sizes remain scrollable and are not claimed to fit one page.

Menu timer now runs screenCycle and gameUpdateScreen only, skipping simulation updates (wind/moons/balloon/turns). Animation continuity and drag scrolling still need hands-on verification. Simulator and signed phone builds passed (`/private/tmp/ultima4-panel-polish.log`, `/private/tmp/ultima4-panel-polish-phone.log`). devicectl confirmed phone installation.

### Startup navigation and keyboard dismissal

User screenshots show legacy title-menu navigation and native name entry keyboard without dismissal; report landscape map filling screen. Screenshots depict title/new-game background rather than gameplay map, so gameplay geometry regression is not established. Added automatic native menu when IntroController is active in INTRO_MENU, guarded against nested timer entry; existing j/i/r/c/a behavior retained. Advanced settings/About still use legacy paths. Added44pt keyboard accessory Hide Keyboard to native name/topic field, preserving typed text. Legacy keyboard scaling ignores native panels and hide callbacks when legacy keyboard is not active, preventing it from distorting native-panel layout.

Simulator build passed (`/private/tmp/ultima4-startup-keyboard.log`), installed/launched without --skip-intro for testing. Mac locked before inspection. Startup selection, keyboard hide/show, intro fallback geometry still unverified. Phone remains previous panel-polish build; these changes have not been installed there. Current phone workaround: keyboard J for Journey Onward, I for new game; nonempty name submits with keyboard checkmark.

### Verified native startup and keyboard-frame recovery

Resumed after the usage interruption and reproduced the user's new-game path. The previous native-keyboard guard was incomplete: SDL's own keyboard observer still resized the root view, and rotation could record that temporary frame as the new normal frame. Native keyboard show/hide now restores the SDL root to the overlay bounds after SDL processes the notification, and control layout never learns a new full frame while a native panel exists. The native panel raises itself after keyboard transitions so it cannot be covered by the restored SDL view. The accessory is now a compact custom 44pt bar with an explicit Hide Keyboard action.

iOS now enters the native tappable startup menu immediately; Watch introduction retains access to the original sequence. Simulator runtime verified the five startup choices and New game → name prompt. A screenshot with the keyboard open verified the title background remains full-sized, the name field and Cancel creation stay visible, and Hide Keyboard is exposed above the keyboard. Simulator accessibility becomes unavailable while the software keyboard owns focus, so actual dismissal and rotate-while-keyboard-open still need hands-on verification. Both simulator and signed device builds succeeded; devicectl confirmed installation and launch on the paired iPhone 14 Pro at 20:44. The phone now contains this revision.

### Removed persistent SDL transforms and legacy About UI

New phone screenshots proved the prior fix was insufficient: About still exposed the DOS information screen with gameplay controls, and a keyboard/rotation sequence could leave both SDL and controls at a persistent reduced transform. The UIKit overlay no longer changes the SDL root view frame or transform for any keyboard event; SDL exclusively owns its window geometry. Native text-entry panels continue to use `keyboardLayoutGuide` and raise themselves after keyboard transitions. This removes the competing frame authorities that caused the persistent 70% scale.

The startup menu now offers Journey onward, New game, Watch introduction and About. About is a native sheet; the unfinished legacy Configure screen is no longer exposed from the primary mobile menu. During the original introduction, all gameplay controls are hidden and a single 48pt Skip button remains. Simulator runtime verified native About in portrait and landscape, the full-size landscape introduction, and the intentionally aspect-fitted portrait introduction without overlaid gameplay controls. The signed device build succeeded and devicectl confirmed installation. Two automatic launch attempts then failed due an invalidated/timed-out device control connection; the installed app can be opened manually on the phone. Rotate-while-native-keyboard-open still requires on-device verification.

### Startup handoff and panel pan gesture

User reported the startup background frozen at Loading, Journey Onward leaving its menu over the loaded world, and spell-sheet scrolling becoming unresponsive at the bottom. The forced iOS skip path now calls `updateScreen()` after intro views are initialized, drawing the title/menu background before presenting UIKit. The startup presentation guard is reset in `IntroController::init`; after Journey Onward or completed creation marks the intro controller done, the guard stays raised until destruction so a timer cannot open a second menu over gameplay. Simulator runtime verified a drawn title background and Journey Onward transitioning to an unobscured Britannia HUD. A leaked Skip label on the Return button was caught during this check and reset to Return whenever gameplay is active.

Panel scrolling now uses a card-level vertical pan recognizer and disables the scroll view's competing built-in pan recognizer. It clamps every update to content bounds and applies a short velocity-based finish. This makes a drag originating over a button cancel the tap and scroll consistently; direct gesture testing remains for the physical phone because the simulator accessibility interface used here does not expose swipe injection. Simulator/device builds and all four portable suites passed. After one transient device connection reset, devicectl confirmed installation and launch on the paired iPhone 14 Pro at 22:19.

### Landscape journal placement and deterministic panel dragging

The landscape text visible over the map in the user's screenshot was the live recent-command log. Widening the right-side sheets had left that log at the sheet's overlapping map origin. It now occupies the right utility column above the action controls and hides while a panel is open. Simulator runtime verified the resulting landscape gameplay composition and opened both the journal source list and a conversation detail: the journal stays anchored as a right-side overlay while map geometry, left status and D-pad remain fixed.

The velocity-based scroll finish could still be animating when the next drag began, which made successive swipes jitter or appear unresponsive. Panel dragging now removes any stale layer animation and tracks each incremental finger translation directly, without queuing a momentum animation. This change is compiled and installed but still requires a physical touch check because the available simulator accessibility driver cannot inject swipes. The simulator and signed device builds succeeded; all four portable suites passed. devicectl confirmed installation and launch on the paired iPhone 14 Pro at 22:28.

### Immediate journal navigation and clarified world commands

Physical testing showed the long journal could reach its lower buttons but leave Back to places temporarily unresponsive after a swipe. Back and Close buttons are now explicitly excluded from the card-wide pan recognizer, so they receive ordinary taps immediately. Panel bounce is disabled and manual scrolling remains clamped to the exact content range, preventing the sheet from moving beyond its final controls. Simulator accessibility verified direct Conversation in Jhelom → Back to places navigation and exposed the navigation identifiers; physical swipe-followed-by-tap behavior still needs the user's confirmation.

The recent activity area now retains six lines in both orientations. Landscape gives it a taller right-utility frame. Portrait uses compact 12-point text in the measured gap between the fixed map and the top of the control cluster; simulator runtime verified six populated lines without overlapping either control column.

The ambiguous Enter and Return controls are now Go In and Continue. Go In still dispatches Ultima IV's E command and includes an accessibility hint naming town, castle, dungeon and shrine entrances. Continue dispatches Return for advancing text or confirming a prompt. Both command columns are 76 points wide so the labels remain legible in portrait and landscape. Simulator and signed-device builds succeeded, all four portable suites passed, and devicectl confirmed installation and launch on the paired iPhone 14 Pro at 22:44.

### Consistent command-button typography

The Spells and Continue controls were created initially as one-character keyboard-symbol buttons and retained their 22-point symbol font after receiving word labels, making them visibly larger than adjacent actions. Gameplay now normalizes every word-based action button to the same 15-point bold font on each HUD update. Simulator portrait runtime verified consistent sizing across Party, Spells, Journal, Menu, Go In, Continue, Talk and Wait. Simulator/device builds and all four portable suites passed; devicectl confirmed installation and launch on the paired iPhone 14 Pro at 22:50.

### Seamless nested-panel handoff

Button selections previously removed the active UIKit panel before the synchronous engine input loop constructed its next panel. That empty interval exposed the D-pad and underlying framebuffer, producing the reported blink in conversations and other nested menus. A completed panel is now made noninteractive and retained as a transition cover. The next panel is fully constructed above it and then removes it; a 120ms fallback removes the cover when the action intentionally returns to gameplay. Completed direction panels are excluded from further D-pad submissions during that fallback interval.

Simulator runtime verified journal source → conversation detail → source list transitions with the gameplay controls hidden throughout the settled states. Transient frame capture is unavailable through the accessibility driver, so physical visual confirmation remains useful. Simulator/device builds and all four portable suites passed; devicectl confirmed installation and launch on the paired iPhone 14 Pro at 23:04.

### Native scroll ownership

Physical testing showed a roughly ten-second interaction lock after beginning a drag in both long Britain history and a long spell list. The shared cause was the card-wide custom pan recognizer: it owned touches for the entire panel, including buttons, while moving a separate scroll view whose own recognizer was disabled. Under the engine's nested input loop, that split ownership could keep subsequent controls in the pan recognizer's cancellation path.

Removed the custom recognizer and restored the UIScrollView pan recognizer as the sole owner of scrolling and button cancellation. The scroll view now delivers touches immediately, explicitly cancels button tracking when a drag wins, locks to the vertical axis, uses fast native deceleration, and keeps bounce disabled. This applies to journal, spellbook, party, menus, conversations, shops and every other shared native panel. Physical confirmation remains necessary because the simulator accessibility driver cannot inject drag gestures. Simulator/device builds and all four portable suites passed; devicectl confirmed installation and launch on the paired iPhone 14 Pro at 23:30.

### Paged Britain history and spellbook

Physical testing still reproduced the long-panel failure after both custom and native scroll implementations, so gesture ownership alone was not the cause. SDL2 runs the game on UIKit's main thread and explicitly pumps both default and UITracking run-loop modes; long UIScrollView content inside the engine's nested synchronous input loop remains an unreliable architecture for core navigation.

The two reported long paths no longer require scrolling. Journal sources are limited to four per page, and a selected source presents one recorded passage at a time with Entry X of Y plus visible Previous entry, Next entry, Back to places and Close journal controls. Existing saved history is read unchanged; only its presentation is paged. The spellbook now presents three spells per page with Next/Previous/Close controls, and spell details use compact line spacing. Simulator runtime verified a complete no-scroll spell page and forward paging while retaining fixed map geometry. Britain-specific multi-entry presentation depends on the user's longer physical archive and awaits confirmation. Simulator/device builds and all four portable suites passed; devicectl confirmed installation and launch on the paired iPhone 14 Pro at 23:39.

### Scrollable NPC journal and distinct spell navigation

The journal is no longer implemented as a nested synchronous choice loop. A dedicated read-only UIKit browser returns control to the engine immediately, allowing its table and transcript scroll views to receive ordinary main-run-loop touch tracking. Its index is grouped into People, Visions and writings, and Earlier conversations; people are grouped by NPC rather than time or visit. Selecting a person opens one continuous transcript with recorded topic captions and supporting place metadata. Search covers names, places, discovered topics and recorded text, and the browser retains each transcript's position while it remains open.

TopicJournal format v3 adds optional speaker, place, content-kind and initiating-topic fields while retaining v1/v2 load compatibility. Newly observed NPC replies now store the talker's name and selected topic. Older archives remain readable under Earlier conversations because their original location-only source cannot be safely attributed to an NPC. Shrine visions retain their own section.

Spell pages retain three choices but now render them as lettered, left-aligned spell cards with mixture and MP status on a second line. Previous and Next occupy stable outlined positions, unavailable endpoints remain visibly disabled, a letter-range indicator reports the page, and Close is separated from both spells and paging. The portrait spell sheet expands upward enough to keep all three roles visible simultaneously.

All four portable suites pass, including new v3 metadata round-trip and v2 migration coverage. The complete iPhone Simulator build succeeds. Runtime verification on iPhone 14 Pro Simulator confirmed NPC/category grouping, continuous transcript presentation, topic captions, search filtering, disabled Previous on the first spell page, forward paging from A–C to D–F, and visible spell/navigation/dismissal controls in portrait. Physical-device touch behavior and accessibility-size layout remain to be verified.

### Main-thread-safe native scrolling

Phone screenshots exposed two problems in the new scrollable views: synchronous mobile commands could enter the engine's nested input loop directly from a UIKit button callback, and plain journal card containers had ambiguous intrinsic heights. The first could starve or re-enter UIKit gesture tracking; the second stretched short journal passages into oversized cards.

UIKit actions now enqueue semantic commands into SDL's event loop, while topic and journal view construction is marshalled explicitly onto the UIKit main thread. Native scroll views use delayed control touches, cancellation when a drag wins, normal deceleration, bounce feedback, and visible indicators. Journal cards are intrinsic-height stack views rather than unconstrained plain views.

Original-game hard wraps are collapsed within paragraphs for mobile display, while intentional blank-line paragraph breaks remain. The simulator verified a long NPC transcript scrolling from its opening entries to its final entries and a synchronous choice panel scrolling from clipped content to its final action. Both remained immediately responsive to subsequent navigation.

### No lingering momentum and no false scrolling

Physical testing disproved the prior assumption that moving command dispatch out of the UIKit callback was sufficient. Phone screenshots showed scroll indicators remaining active after the finger lifted; while UIKit remained in that incomplete momentum state, it suppressed every button in the panel.

All native panel scroll views now stop at the exact finger-release offset instead of entering deceleration. `alwaysBounceVertical` is disabled, so menus and journal indexes whose content fits are stationary and show no false scrolling. Overflow remains directly draggable.

Ordinary NPC conversation topics no longer depend on scrolling: four topics appear per page, Goodbye remains available on every page, and a distinct fixed Previous topics / More topics row changes pages. Conversation sheets expand upward in portrait to keep the transcript, topics, navigation and custom-question action visible. The simulator verified the fitting pause menu remains stationary after an attempted scroll; physical confirmation of the no-momentum journal drag remains required.

### Gesture-free conversation and journal reading

Further iPhone testing showed that even zero-deceleration UIScrollView drags could remain in UIKit's tracking state under SDL's nested main-thread event pump. The persistent scroll indicator and simultaneously unresponsive buttons confirmed that the problem was run-loop ownership, not content sizing or momentum settings.

Conversation pages now have scrolling disabled entirely. Their four-topic layout is fixed, and Goodbye is a persistent 44-point action in the top-right header instead of a choice that moves among the topic rows. The journal keeps each NPC's entries in one continuous compact transcript, but direct dragging is disabled; persistent Previous and Next controls move one clamped viewport at a time with no animation, bounce, deceleration, or gesture state. A transcript that fits disables both controls. The fitting pause menu remains non-scrollable.

All four portable suites pass and the signed arm64 iphoneos build succeeds. The updated app was installed on the paired iPhone 14 Pro at 11:39; automatic launch was denied because the phone was locked, so it must be opened manually for the physical interaction check.

### Orientation-specific conversation geometry

Conversation geometry now follows separate portrait and landscape compositions. Portrait height is calculated from the current dialogue and controls (with a compact minimum) instead of reserving a fixed 560-point sheet. Landscape uses nearly the full safe height and a wider two-column composition, placing dialogue beside the topic controls so the short landscape axis no longer clips the fixed, non-scrolling interaction set. The fixed Goodbye header action remains in both orientations.

All four portable suites and the signed arm64 iphoneos build pass. The updated build was installed and launched successfully on the paired iPhone 14 Pro at 11:49 for physical orientation testing.

### Stable six-topic conversation composition

Physical screenshots showed that content-derived portrait height still changed between topic pages, while the landscape two-column split left a large empty dialogue column and forced controls into an unnecessarily narrow column. Conversations now reserve a fixed 390-point portrait sheet and show six topics per page in three two-column rows. A flexible spacer pins a single footer row to the bottom in the stable order Previous, Something else, Next; endpoint paging controls remain visibly disabled rather than moving or disappearing.

Landscape returns to one coherent vertical reading flow inside a wider panel that uses nearly the full safe height. This gives topic labels the width they need, keeps the same header/topic/footer hierarchy as portrait, and avoids the awkward sidebar appearance. All conversation scrolling remains disabled. All four portable suites and the signed arm64 iphoneos build pass; the build was installed and launched on the paired iPhone 14 Pro at 11:56.

### Location-first journal index

The journal's first screen now uses locations as its top-level sections and lists NPCs beneath each location. An NPC's entries within that location remain one continuous encounter-order transcript, with no time- or visit-based subdivision. Visions, writings and unattributed conversations appear as explicitly labeled rows under their recorded location. Version 1/2 entries recover a location from their legacy `Conversation in …` source when possible; anything that cannot be attributed is collected in a final Location not recorded section.

Location headings sort predictably, search continues to filter across names, places, topics and text, and the selected row no longer repeats its location as redundant secondary text. This intentionally supersedes the earlier NPC-first index direction in the quality-of-life notes. All four portable suites and the signed arm64 iphoneos build pass; the updated build was installed and launched on the paired iPhone 14 Pro at 12:03.

### Journal location drill-down and bounded lists

The location headers and NPC rows previously shared one table, so a mature journal would still become a long first screen. The journal is now an explicit three-level hierarchy: a root list of locations, a separate list of NPCs and other records at the chosen location, then the selected transcript. Location rows summarize people and note counts; the back button returns one level at a time.

Both list levels avoid drag scrolling and use bounded pages: eight rows in portrait and three in landscape, with fixed Previous, page count and Next controls. This retains predictable touch behavior under SDL's nested event loop and ensures that additional towns or NPCs cannot grow beyond the screen. Search filters the hierarchy without flattening it. All four portable suites and the signed arm64 iphoneos build pass; the updated build was installed and launched on the paired iPhone 14 Pro at 12:08.

### Persistent party health and condition roster

The exploration and combat HUD now receives a structured snapshot of all eight
possible party members and renders every current member as a native status cell.
Each cell includes party order, name, numeric current/maximum HP, a proportional
HP bar, and a color-independent condition abbreviation for Healthy, Poisoned,
Asleep, or Dead. Combat's active member additionally receives a chevron and a
high-contrast border. VoiceOver exposes the full name, health, condition, and
active-combatant state.

Portrait reserves a four-column by two-row strip between the world and lower
controls, including on short phones. Landscape uses a two-column by four-row
rail above the D-pad, while location and resource status occupy the opposite
utility rail. Recent activity remains available as a three-line, half-map-width
translucent world overlay in both orientations. Empty separators do not consume
one of its lines, and longer entries word-wrap. Tapping a status cell sends a parameterized
semantic action through SDL's event queue and opens that companion's detail
panel directly without advancing a turn.

All five portable mobile suites pass and the arm64 iPhone Simulator Release
build succeeds. Runtime verification on the iPhone 14 Pro Simulator confirmed
portrait and landscape composition, supported-orientation rotation, VoiceOver
values, and direct roster-to-Lindsay detail navigation. The available saved
runtime fixture contains one party member; an eight-member layout and condition
variants still require runtime visual verification. The signed build was then
installed and launched successfully on the paired physical iPhone 14 Pro at
17:47.

The revised recent-activity geometry was visually verified in portrait and
landscape on the iPhone 14 Pro Simulator: the longer startup message wraps to
two lines and a newer status remains visible on the third. All five portable
mobile suites and both simulator and signed arm64 iphoneos builds pass. The
updated build was installed on the paired iPhone 14 Pro at 18:05; automatic
launch was denied because the phone was locked.

### Exploration-based Britannia map

Ultimatum and Assisted now provide a visible Map action during world
exploration; Classic keeps it disabled by default, and Experience customization
can override the profile choice. The action opens a temporary native Britannia
overlay rather than competing with the gameplay viewport. It begins zoomed and
centered on the party, provides explicit Zoom Out, Zoom In, Up, Down, Left, and
Right controls, and includes a full-map zoom level for later exploration. The
map intentionally has no pan, scroll, or pinch recognizer because UIKit gesture
tracking can stall under SDL's nested main-thread event pump.

The reveal model records the same 11-by-11 terrain region visible in ordinary
overworld play. Unseen cells remain dark, terrain uses a compact semantic color
palette, and a small fixed-size dot marks the current or last overworld
position. The dot alternates red and blue using Core Animation; Reduce Motion
instead receives a stationary red dot with a blue and white outline.
No towns, dungeons, shrines, or other locations are exposed before their terrain
has been seen. The exploration bitmap is adventure-owned, loads legacy
checkpoints as an empty map, and is written and retained atomically with future
checkpoint generations.

All five portable mobile suites pass. The arm64 iPhone Simulator build passes,
and runtime verification on the iPhone 14 Pro Simulator confirmed the Map
affordance, modal accessibility hierarchy, party-position description,
party-centered zoom, and landscape/rotation-safe layout. A follow-up simulator
check verified immediate response after repeated discrete zoom, pan, and close
actions, including correctly disabling pan controls at full-map scale. The signed arm64
iphoneos build also passes and was installed on the paired iPhone 14 Pro at
18:26; automatic launch was denied because the phone was locked.

After physical feedback reproduced the project's broader UIKit gesture-tracking
stall in the map, its scroll view and every touch gesture were removed. The
replacement uses six ordinary 44-point buttons for zoom and directional pan;
simulator testing exercised repeated zoom, pan, full-map disabled states, and
dismissal without a stuck tracking session. The smaller flashing-marker build
was signed, installed, and launched successfully on the paired iPhone 14 Pro at
18:42.

The map controls now use two spatially distinct groups: a conventional cross
D-pad at the lower left for panning and a vertical plus/minus zoom pair at the
lower right. The flashing marker is eight points at the full-world overview and
scales progressively with the map to 12.5 points at the default zoom and 18.5
points at maximum zoom. Simulator runtime verified both the default and
full-world compositions, including the grouped controls and marker scaling. The
signed build was installed and launched successfully on the paired iPhone 14
Pro at 18:46.

### Context-sensitive current-tile action

The former Go In HUD control is now an engine-described context action. During
exploration it changes among Enter, Board, Dismount, Disembark, Ascend, Land,
Climb, Descend and Open Chest according to the party's current tile and
transport state. With no unambiguous action it reads Interact and is disabled;
unlike the legacy smart Enter enhancement it never silently falls back to a
turn-consuming Search.

The resolver is side-effect-free and independently tested. Button activation
queues a semantic SDL event, and the engine resolves the action again when that
event is consumed so stale HUD state cannot execute an outdated command. Talk
and directional door commands remain explicit while adjacent-target and direct
world-tap behavior are developed separately. All five portable mobile suites
and the arm64 iPhone Simulator Release build pass. The build was installed and
launched in the iPhone 14 Pro Simulator; that simulator had no existing
adventure, so current-tile label transitions and command turn accounting still
require a gameplay fixture or physical-device verification.

### Controls-only character creation prompts

The native character-creation layer no longer repeats story passages or virtue
questions that are already rendered by the original game. Reading stages show
only Continue, and moral choices show only Choose A and Choose B. In portrait,
the controls use a transparent strip below the aspect-fitted game image. In
landscape, they use a compact upper-right safe-area control that normally sits
in the right letterbox rail and avoids the original text area at the bottom.
The original prompt remains in the
accessibility hierarchy as static text, followed by the
available controls, so removing the visible duplicate does not remove the
VoiceOver equivalent.

Simulator end-to-end verification created `Test`, advanced all 24 story pages,
exercised both Choose A and Choose B, completed the seven-round virtue tree,
entered gameplay, relaunched the app, and loaded the character through Journey
onward. This uncovered and fixed a new-adventure snapshot regression: snapshot
publication had begun requiring `explored-map.dat`, but character creation did
not write the initial all-unrevealed map. Creation now writes that
adventure-owned metadata before publishing. Landscape creation controls,
landscape gameplay, supported upright portrait recomposition, and persisted
reload were visually verified on the iPhone 14 Pro Simulator. All five portable
mobile suites and the arm64 iPhone Simulator Release build pass.

### Bump and direct adjacent interaction

Exploration now has two independently configurable interaction translations,
both enabled by default on iOS. A deliberate, blocked movement into an exact
cardinally adjacent friendly conversable NPC starts the existing Talk flow; the
same attempt against an unlocked door calls the existing Open action. Holding a
direction continues movement repeat but cannot repeat an interaction. Tapping
one of the four adjacent map tiles invokes the same resolver without first
requiring collision. Explicit Talk, current-tile Interact, and Explore commands
remain available.

The shared resolver excludes combat, dungeons, panels, flying and non-foot/horse
travel, collision override, and hostile or non-conversable actors. Locked doors
produce guidance but never consume a key automatically. Direct events carry
the originating map and coordinates and are revalidated when consumed, so
queued taps cannot redirect after movement. A dedicated movement-result bit
preserves blocked-position semantics while suppressing redundant Blocked
feedback and retaining one-turn accounting.

The pause menu retains its six-item portrait limit and exposes Bump to interact,
Tap adjacent targets, and a plain-language guide under Controls. Portable tests
cover the resolver's positive and exclusion cases. The arm64 Simulator Release
build passes. On the iPhone 14 Pro Simulator, the persisted `Test` adventure was
loaded, both settings were toggled and restored, bump-to-talk and direct
tap-to-talk were exercised against the same Jhelom guard, and the resulting
conversation remained intact across portrait-to-landscape rotation. Unlocked
and locked door runtime fixtures remain to be exercised; their resolver paths
are covered by the portable suite.

### Landscape panel and locked-door feedback follow-up

Physical-device feedback showed that landscape conversations covered more of
the world than their content required, the spellbook's Close action could fall
below its scroll position, and a safely rejected locked-door bump was silent
under filtered movement messages. Landscape conversations now use a compact
380–460 point composition while retaining a wider accessibility-text layout.
The spellbook now owns a fixed footer with Previous, Next, and Close actions;
only the spell list scrolls, so dismissal is always visible. A locked-door bump
or adjacent tap remains blocked, consumes no key, and writes `Locked door. Use
Explore > Unlock.` to recent activity instead of generic blocked feedback.

All five portable suites and the arm64 Simulator Release build pass. Simulator
visual verification confirmed the narrower landscape conversation and the
fixed spellbook footer without scrolling to Close. Locked-door messaging is
resolver-covered and still needs a fixed runtime door fixture.

### Universal multiple adventure slots

Classic, Ultimatum, and Assisted now share three independent adventure save
slots as part of the Core Mobile Standard. Slot 1 retains the original
`snapshots` root and legacy fallback, so existing adventures require no file
migration. Slots 2 and 3 use separate immutable generation trees with their own
CURRENT, PREVIOUS, journal, explored-map, retention, and recovery state. The
active slot is a device-level atomic pointer; an empty secondary slot can never
fall through to Slot 1's legacy party files.

Journey Onward lists occupied slots by slot number, Avatar name, and party
size. New Game lists all slots, labels empty slots, and requires explicit
confirmation before replacing an occupied slot; declining returns to the slot
picker. Manual and periodic save feedback identify the active slot. The new
portable suite verifies default legacy selection, atomic selection persistence,
range rejection, occupied detection, path isolation, and independent snapshot
publication. All six portable suites pass.

The arm64 Simulator Release target and signed arm64 device target build
successfully. Runtime verification on
the iPhone 14 Pro Simulator preserved the existing Test Mage adventure as Slot
1, displayed its two-member summary, loaded it, and showed `Save to Slot 1` in
the pause menu. The New Game picker displayed occupied Slot 1 plus empty Slots 2
and 3, and the occupied-slot warning was visually verified without confirming
replacement. Creating, saving, relaunching, and alternating between two real
slots remains the next end-to-end check. The physical phone was not changed.

### Native Debug Tools and pause-menu capacity

Local development and preview builds expose a UIKit Debug Tools screen from the ordinary and
battle pause menus. Its root is a six-row category list—Session, Navigation,
Party & Inventory, World, Diagnostics, and Danger Zone—and each destination is
a short native page with Back and Done. Context-sensitive commands are disabled
with an explanation, toggles report their current engine state, and destructive
commands are red and require confirmation. The first adventure-changing action
in a Debug Tools session offers to create a recovery checkpoint for the active
slot; actions reuse the engine's semantic cheat operations rather than emulating
desktop keyboard input. Public builds do not expose the entry.

Resume moved from the compact pause grid into a fixed 44-point header action,
matching conversation dismissal. This leaves six full-size grid positions for
Save, Explore, Travel, Experience, Controls, and Debug Tools without adding a
scroll view or reducing touch targets. If more top-level actions are added, the
next step is a category-based pause hub rather than further compressing the
overlay.

The arm64 iPhone Simulator Release build passes. Runtime verification on the
iPhone 14 Pro Simulator confirmed the new pause composition, all six Debug Tools
categories, the four-switch Session page with a single VoiceOver element per
switch, Back and Done navigation, the longest seven-row Party & Inventory page,
the paged 31-destination location picker, and recovery confirmation. Collision
was toggled on, persisted across panel reconstruction, then restored off; no
persistent test mutation was confirmed. The signed arm64 device target also
passes, and the preview bundle was installed on the paired iPhone 14 Pro.

Debug Tools page changes now use a composited native handoff: the outgoing page
remains visible until the replacement has valid layout, then retires in the
same UI transaction. This removes the one-frame world flash between categories,
pickers, confirmations, and refreshed toggle state. True dismissal still
reveals the pause screen immediately, while commands that deliberately request
a direction dismiss the native surface so the HUD prompt remains visible.
Frame-by-frame Simulator capture of root → Session → root contained only those
three native surfaces, and the transport direction/cancel round trip was also
verified without changing the adventure.

### Player-created exploration map pins

The exploration map now supports up to 24 player-created pins on revealed
overworld tiles. The explicit Pin Here action adds or renames the current or
last overworld position, and existing gold diamond markers have 44-point tap
regions for rename and remove actions. This adds no gesture-only critical path.
Classic defaults the optional feature off; Ultimatum and Assisted default it on,
and the Experience screen exposes a reversible Player map pins override without
discarding stored pins.

Pins live in a validated, versioned `map-pins.dat` file within each adventure's
immutable checkpoint generation. Missing files remain valid for legacy saves;
malformed data fails checkpoint preflight, while saves use atomic replacement
before snapshot publication. New Game initializes empty pin data, checkpoint
retention preserves it, and adventure-slot tests verify pin isolation and
previous-generation recovery. The map explains that edits join the next manual
or automatic checkpoint.

All seven portable mobile suites pass, as does the arm64 iPhone Simulator
Release build. An isolated iPhone 17 Pro Simulator fixture was prepared so this
work does not disturb the active multi-slot test environment. Runtime
verification added and renamed a pin, confirmed its accessible label and
44-point marker in portrait and landscape, manually checkpointed Slot 1,
relaunched, and confirmed the label reloaded from the published generation. The
first input pass exposed physical-keyboard events leaking through the native
label field into gameplay; the SDL event boundary now suppresses game key
events while that editor is active, and the corrected behavior was verified.
Removal is covered by the portable suite but has not been invoked in the runtime
fixture. The active iPhone 14 Pro multi-slot simulator and physical phone were
not changed during Simulator verification. At the user's subsequent request,
the current source was built and development-signed for the existing
configured preview bundle; `devicectl` confirmed installation on the
paired iPhone 14 Pro. Reusing the bundle identifier preserves its app container.
The app was not auto-launched, avoiding interference with the user's parallel
slot-validation session.

Physical-device testing then exposed an alert-transition race: committing Add
Pin could leave the interface unresponsive, and opening Edit immediately after
a successful add could wedge it more reliably. The alert handlers had rebuilt
the map and cleared their re-entry guard before UIKit finished dismissing the
native editor. Pin mutation now completes first, but the map refresh, input
unblock, and editor re-entry wait until the alert is actually off-screen. The
corrected simulator build completed an edit and immediately reopened the same
pin editor, all seven portable suites pass, and the signed build was installed
over the configured preview bundle on the paired iPhone 14 Pro. Existing app
data was preserved.

A second physical pass found that Add and Remove still took a few seconds while
Edit was immediate. That delay matched the conservative 120-frame dismissal
polling introduced with the race fix. The polling and two-second fallback are
now gone: UIKit's transition coordinator or direct dismissal completion performs
the refresh and unlock on the actual transition boundary. Portable suites and
simulator/device builds pass, and the follow-up signed build was installed on
the paired iPhone 14 Pro for physical confirmation.

The remaining physical delay was specific to closing the text-entry editor:
Add and Remove dismiss the keyboard and alert, while opening Edit does not. The
pin editor now ends text entry and performs a non-animated modal dismissal,
then refreshes from that dismissal's completion; pin mutation remains in-memory
until the ordinary checkpoint path. The native title screen also displays the
bundle's `CFBundleVersion` at the top center so physical feedback can identify
the exact binary. Local device build scripts now forward explicit marketing and
build-version values into CMake. Simulator layout verified the centered label,
and paired-device inventory confirms the configured preview bundle version 1.0,
Build 4 is installed. All seven portable suites and both iOS builds pass.

Build 4 physical testing confirmed that the stall was not an animation delay:
every `UIAlertController` action could highlight without completing while SDL's
nested iOS event loop was active. The pin editor no longer presents a view
controller. It is now an inline, map-owned overlay with 44-point Add/Save,
Remove, and Cancel controls, keyboard-aware portrait and landscape layout, and
an accessibility-modal focus boundary. Each action mutates or abandons the
edit synchronously and removes the overlay immediately; reaching the pin limit
also reports inline instead of opening another alert. Simulator runtime testing
confirmed that Cancel and Save both close immediately, that the map remains
responsive, and that the marker label refreshes and can be reopened. All seven
portable suites and both iOS builds pass. The title label reports Build 5, and
paired-device inventory confirms the configured preview bundle version 1.0,
Build 5 is installed on the iPhone 14 Pro.

Follow-up map review found two presentation gaps. The exploration palette's
deep-water test recognized `water` but not the base tileset's `sea` tile, so
unmatched sea cells inherited the default grass green. Water classification now
uses the tile rule (with name fallbacks), after checking shallows, so sea is dark
blue and shallows remain distinct. Debug Tools also exposed its legacy GEM-view
cheat as an enabled mobile switch even though the GEM renderer has not received
a native adaptation. Build 6 temporarily made that row informational and
non-activating rather than entering the DOS view. Simulator review visibly
confirmed dark-blue sea alongside lighter shallows, all seven portable suites
and both iOS Release builds passed, and the signed Build 6 was installed on the
paired iPhone 14 Pro.

### Native GEM/Peer map

The iOS Peer command no longer enters the engine's DOS `VIEW_GEM` renderer.
It builds a native, full-area map from the active map and level, pauses through
the existing waitable-controller boundary, and restores the prior view only
after the player selects Return. Ordinary gem use still checks and consumes one
gem before opening the map; Lycaeum telescope views still enter the selected
settlement and return to the parent map afterward. The panel starts with the
whole area visible, offers explicit 44-point zoom and pan controls, centers a
color-independent player marker, supports Return/Escape/Space hardware input,
uses location- and dungeon-level-specific accessible labels, and fully obscures
the legacy renderer beneath it.

Debug Tools now exposes Preview GEM map as a non-consuming action backed by this
same native path. Simulator runtime verification covered the Britannia world
map, Lycaeum settlement map, and Deceit Level 1 dungeon map; Return restored the
Debug Tools page or the prior dungeon view, and a real dungeon Peer invocation
displayed `Peer at a Gem!` before the native surface. All seven portable mobile
suites and the arm64 Simulator Release build pass. Build 7 is the corresponding
phone build; the signed configured preview bundle is installed on
the paired iPhone 14 Pro with its existing app container preserved.

### Discovery-gated place markers

The Britannia exploration map now records a system marker only after a
successful overworld portal entry. This deliberately avoids scanning the
world's portal configuration, and unsuccessful shrine/Abyss entry, GEM views,
telescope previews, and Debug Tools previews do not reveal a destination.
Successful entry classifies towns (including ruins), castles, villages,
shrines, and dungeon entrances and records the entrance coordinate and public
place name. Loading a legacy checkpoint while already inside a place safely
backfills that one proven entrance. A marker is displayed only when its
entrance cell is also present in the adventure's explored bitmap.

System markers are read-only and visually separate from editable gold player
pins: town circle, castle square, village ring, shrine star, and dungeon
triangle, each with a secondary color distinction, a 44-point touch region,
and a type/name accessibility label. Tapping announces the known name and type;
it cannot edit or remove the marker. No exact coordinate is exposed in the
marker label.

Discoveries persist in a bounded, validated, versioned
`map-discoveries.dat` inside each immutable adventure checkpoint. Saves use an
atomic temporary-file replacement; New Game initializes an empty set; missing
files migrate as empty legacy metadata; malformed files fail checkpoint
preflight transactionally. Snapshot retention recognizes the file, and the
adventure test covers generation isolation and previous-generation recovery.
All eight portable mobile suites pass. Both arm64 Simulator and signed device
Release targets compile as Build 8. Runtime visual verification was blocked by
the macOS Simulator application's Apple-event timeout; `devicectl` nevertheless
confirmed Build 8 installed over the configured preview bundle on the paired
iPhone 14 Pro with its existing app container preserved.

Physical Build 8 review confirmed the intended discovery state after visiting
Moonglow and Britain: two small revealed terrain islands, town-circle markers,
and the party marker overlapping the last overworld position. It also showed
that tap-only identification was too cryptic. Build 9 therefore displays the
known place name beside each visible system marker by default. Labels use a
compact high-contrast backing and marker-category border, try right, left,
above, and below placements to avoid one another, move with zoom/pan, and never
appear for hidden or offscreen markers. A visible 44-point Hide Labels / Show
Labels control at the lower right toggles them for the running app session.
The label is decorative for accessibility; the existing marker remains one
44-point VoiceOver target with the place name and type. All eight portable
suites and both arm64 iOS Release targets pass as Build 9. Simulator visual
automation remains unavailable because Simulator.app continues to time out at
the macOS Apple-event boundary. `devicectl` confirms Build 9 installed over the
preview bundle on the paired iPhone 14 Pro with its app container preserved.

### Tap-to-expand overworld minimap

The former top-corner Map text button is now a live north-up minimap, preserving
the same single-tap path into the full Britannia map without adding permanent
HUD chrome. It renders a 21-by-21 local window centered on the party using the
same explored bitmap and terrain palette as the large map. Unknown cells stay
dark. Discovered places use category-specific pixel shapes and colors; the
party is a white 3-by-3 marker with a red center, so neither depends on color
alone. The compact view intentionally omits labels and player pins.

Portrait uses the existing 68-by-52 top-right allocation above the world view;
landscape uses a 76-by-64 preview in the right utility rail. Both retain at
least a 44-point tap target, a visible button border, nearest-neighbor pixels,
and an accessibility label explaining that activation opens the full map. The
preview updates after movement but hashes its tiny raster to avoid rebuilding
an unchanged UIKit image. It appears only during ordinary overworld control and
automatically hides in settlements, shrines, dungeons, combat, keyboard input,
and native panels. All eight portable suites and both arm64 iOS Release targets
pass as Build 10. `devicectl` confirms Build 10 installed over the preview bundle
on the paired iPhone 14 Pro with its app container preserved. Visual automation
is still blocked by the existing Simulator.app Apple-event timeout.

Physical Build 10 review showed that UIKit was centering the 21-pixel source
image at its intrinsic logical size instead of scaling it to the intended
preview area. Build 11 gives the minimap button an explicit square image rect:
46-by-46 points in portrait and 58-by-58 points in landscape, centered inside
the unchanged 68-by-52 and 76-by-64 tap targets. The nearest-neighbor filter is
preserved so the enlarged tiles stay crisp. All eight portable suites and both
arm64 iOS Release targets pass as Build 11, and `devicectl` confirms Build 11 is
installed over the preview bundle on the paired iPhone 14 Pro with its app
container preserved.

Physical Build 11 review showed that the landscape minimap sat lower than the
other glanceable status information. Build 12 moves it to the safe-area
top-right while leaving portrait unchanged. The 176-point landscape utility
strip is now intentionally split into a 92-point status column, an 8-point gap,
and the existing 76-point minimap, so multiline status text cannot overlap the
preview. All eight portable suites and both arm64 iOS Release targets pass as
Build 12. `devicectl` confirms Build 12 installed over the preview bundle with
its app container preserved, and physical review confirms the new landscape
placement works as intended.

### Discovery-gated dungeon-floor maps

The exploration system now records visited dungeon cells independently for all
eight dungeons and all eight levels. It deliberately reveals only the exact
tile reached by the party; facing or looking down a corridor does not disclose
untraveled layout. Entering a room records its threshold before the room combat
controller takes over. The current level appears in the same top-corner
tap-to-expand preview used in Britannia, with a white-and-red party marker, and
tapping it opens a native 8-by-8 level map titled with the dungeon name and
one-based level number. Known ladders, rooms, fields, doors, chests, traps,
fountains, altars, and special tiles receive distinct high-contrast colors; the
visible help text explains the most important categories. VoiceOver identifies
the compact control specifically as the current dungeon-level minimap.

Visited cells persist in `explored-dungeons.dat`. The bounded, versioned parser
rejects unknown dungeon ids, out-of-range coordinates, duplicate records, and
over-capacity input without replacing valid in-memory state. Missing metadata
from older checkpoints is accepted as an empty dungeon map. New adventures
start empty, while ordinary saves, autosaves, per-slot isolation, immutable
generation publication, and previous-checkpoint retention all include the new
file without altering `dngmap.sav` or the original game rules. Nine portable
test suites and both arm64 iOS Release targets pass as Build 13. The simulator
launch smoke check reached the Build 13 title screen, and `devicectl` confirms
Build 13 installed over the preview bundle on the paired iPhone 14 Pro with its
app container preserved.

### Dedicated dungeon controls and explored overhead view

Ordinary dungeon exploration now participates in the native mobile gameplay
layout instead of falling through to the legacy framebuffer and its erroneous
introduction-only Skip control. The HUD shows dungeon name, level, facing,
light duration, carried torches, food, gold, party status, recent activity, and
the discovery-gated floor minimap. Its D-pad is relabeled to Forward, Back, Turn
Left, and Turn Right; Forward and Back retain hold-repeat, while turns are
deliberately one-shot. Search and Torch have permanent semantic buttons,
chests and ladders use the existing contextual action, and Journal remains
available through the pause menu.

The former debug-only alternate dungeon renderer is now a visible, no-turn
Overhead / 3D View toggle. It reuses the existing dungeon tileset and movement
model, keeps the party centered, and queries the persisted dungeon exploration
record before drawing every non-party cell. Unvisited cells and every cell while
unlit remain black, so the alternate presentation cannot disclose untouched
layout or secrets. The toggle, movement controls, minimap, and dungeon actions
have explicit VoiceOver labels, hints, and state values in both orientations.

The detailed interaction contract and acceptance criteria are recorded in
`docs/design/DUNGEON_MOBILE_UI_SPEC.md`; the touch-control guide and quality-of-
life roadmap now reflect the shipped paths. All nine portable mobile suites,
the arm64 Simulator Release target, and the signed arm64 device Release target
pass as Build 14. A simulator runtime pass covered portrait and landscape,
dark and lit first-person rendering, Torch and Search, relative movement and
turning, ladder context, the explored floor map, and switching both directions
between first-person and fogged overhead presentation. The final portrait pass
also verified the compact status line and the Climb transition back to Britannia.

### North-up dungeon floor correction

Physical Build 14 feedback exposed two presentation problems that shared the
same underlying fact: every Ultima IV dungeon level is an 8-by-8 wrapping map,
while the classic gameplay viewport is 11-by-11. The first overhead pass filled
that larger viewport with a player-relative sample, so cells repeated across
the edges and the entire scene rotated with facing. The expanded discovery map
also rendered unknown cells as the same black as its surrounding panel, hiding
the real 8-by-8 boundary and making explored fragments look arbitrary.

Build 15 makes the mobile overhead presentation a fixed north-up rendering of
the single 8-by-8 floor. Each cell is drawn at most once, the party moves within
the board, and the D-pad switches to cardinal North, South, West, and East.
A cardinal touch faces the party and dispatches the original forward movement,
preserving collision, floor-edge wrapping, hazards, encounters, and turn cost.
Switching back to 3D restores Forward, Back, Turn Left, and Turn Right.

The expanded floor map now states `North ↑` and `8×8`, outlines the complete
board, and draws subtle cell boundaries so black unvisited cells remain legible
as part of the map. All nine portable suites and both arm64 Release targets
pass. Simulator runtime verification confirmed eastward movement moves the
party sprite right, changing facing south does not rotate the board, explored
cells remain discovery-gated, and the bounded floor map matches the gameplay
orientation.

### Live dungeon hallway line of sight

The north-up overhead presentation now combines persistent traversed cells with
the party's current cardinal line of sight. Open hallway cells remain visible
until the bounded edge of the 8-by-8 floor or the first wall, secret door,
ordinary door, or room entrance. The blocking cell is drawn so the obstruction
is understandable, while cells beyond it remain hidden unless they were
previously traversed. Current sight does not write to `explored-dungeons.dat`,
preserving the distinction between live visibility and exploration history.
All nine portable mobile suites and both arm64 iOS Release targets pass as
Build 16. Simulator verification in Deceit Level 1 showed the entrance's open
east and south hallways immediately, then confirmed that movement updates the
live rays while adjacent walls reveal themselves and conceal cells behind them.
`devicectl` confirms signed Build 16 is installed over
the configured preview bundle on the paired iPhone 14 Pro with its existing app
container preserved.

### Remembered dungeon sight

Build 17 promotes legitimate overhead line of sight into the persisted dungeon
exploration record. Open hallway cells and the first blocking wall, door, room
entrance, or secret door now remain visible after the party moves away, while
cells beyond the blocker remain unknown. Secret doors use the ordinary wall
color on the compact and expanded maps, preventing remembered sight from
disclosing their identity. The expanded-map legend now describes black cells as
unseen rather than unvisited.

All nine portable mobile suites and both arm64 iOS Release targets pass.
Simulator verification captured the entrance sight, moved east to place the
original walls and south hallway outside current line of sight, confirmed they
remained in both overhead and expanded maps, saved Slot 1, relaunched the app,
and confirmed the remembered geometry reloaded. `devicectl` confirms signed
Build 17 is installed over the configured preview bundle on the paired iPhone
14 Pro with its existing app container preserved.

### Dungeon seam visibility

The fixed north-up overhead board now reconciles its presentation boundary with
the dungeon engine's wrapping topology. Cardinal sight rays cross an 8-by-8
floor edge and continue at the opposite edge, so an adjacent wrapped wall or
door that blocks movement is revealed and remembered in its one canonical map
cell. Rays still stop at that blocker, the board neither rotates nor repeats,
and secret doors retain ordinary wall presentation.

A pure wrapping-sight component and regression suite cover the exact Deceit
Level 2 south-edge layout from physical feedback, the equivalent horizontal
seam, blockers on both sides, unobstructed wrapping rows and columns, and invalid
inputs. All ten portable mobile suites and both arm64 iOS Release targets pass
as Build 18, and the rebuilt Simulator app launches into the saved Deceit
overhead view. The signed phone bundle is ready, but installation could not run
because the paired iPhone 14 Pro was unavailable to CoreDevice at completion.

Physical review subsequently confirmed Build 18's dungeon seam correction works
as intended on the iPhone 14 Pro.

### Touch combat targeting and range preview

Build 19 replaces the three visibly dead combat actions with Previous Target,
Next Target, and Clear. Every combatant that is currently reachable under the
active weapon's original cardinal path, range, fixed-distance, and blocking
rules receives a minimum 44-point touch target. The selected combatant is shown
with a distinct ring symbol, an accessible selected state, its name, distance,
and direction in the actor status, and a dashed range path. These cues do not
depend on color alone. Tapping, cycling, or clearing a target does not spend a
turn; the separate Attack button confirms it. Directional targeting remains
available after Clear and for attacks that need an empty tile or an explicit
direction.

The selected Attack path reuses the existing attack resolver, including hit and
damage rolls, weapon loss or return, path blocking, tile effects, messages, and
turn completion. Target state is revalidated against current combat state and
clears when the active character changes or moves, preventing stale-coordinate
attacks. The battle help text describes both touch targeting and the directional
fallback.

Simulator runtime verification covered an initially invalid unarmed target,
ranged target appearance after engine-valid alignment, direct tap selection,
single-target cycling, Clear, selection persistence through portrait and both
landscape orientations, color-independent ring and path presentation, and
selected Attack. Selection, cycling, and clearing retained Test Mage's turn and
HP; Attack ran the original Magic Wand miss resolution and advanced to Test
Companion without opening the direction chooser. VoiceOver exposed target name,
distance, direction, selection, confirmation, and no-turn hints throughout. All
ten portable mobile suites and both arm64 Simulator and signed device Release
builds pass. `devicectl` confirms Build 19 installed over
the configured preview bundle on the paired iPhone 14 Pro with the existing app
container preserved.

### Consistent panel navigation

Menu choices now carry an explicit action, back, or dismiss role instead of
requiring UIKit to infer navigation from button text. Nested Back actions render
in the leading header position, while terminal Close, Resume, Goodbye, Cancel,
Leave, and End actions render in the trailing position with their contextual
labels. Confirmation Cancel buttons remain paired with their commit action, and
the direction-only targeting panel retains its adjacent visible Cancel control.

The migration covers adventure and battle pause menus, Party and equipment,
Spellbook and spell parameters, Explore and Travel, Experience and Controls,
Debug Tools child pickers, save-slot and recovery flows, shops, character entry,
quest items, attack distance, shrines, virtues, and offerings. Spellbook page
controls remain in the footer without competing with Close. Runtime verification
on the iPhone 14 Pro simulator covered Party root and detail, Spellbook list and
detail, save-slot Back, adventure Resume, and Experience Back in portrait. The
portable mobile suites and arm64 Simulator Release build pass.

### Dense paginated planning panels

Party, Spellbook, spell details, and equipment selection now use a dedicated
full-screen dense presentation that disables gesture scrolling. The Party
roster exposes name, condition, HP, and MP on the first line, reserving the
second line for weapon and armour so long equipment names cannot collide with
the vitals. It fits all eight companions in portrait, and its rows pair into
two columns in landscape. The seven-companion reorder flow uses the same
fixed-screen treatment.

Spellbook pages expand from three to eight spells and present them as compact,
full-width one-line rows in portrait with mixed count, MP cost, and any cast
restriction not already explained by a zero mixture count. Landscape uses two
columns so all eight retain the 44-point minimum touch height above the footer.
The existing explicit Previous and Next footer remains the only page-transition
mechanism. Spell details replace the tall prose block
with a compact resource summary, reagent inventory, and cast state; even
Resurrect's six-reagent list fits without scrolling. Character details keep
their actions at the bottom.

Equipment selection shows up to four eligible choices per page, which preserves
full-width 44-point rows and the fixed footer in both orientations, including
rotation while the picker is open. Previous and Next appear only when the
inventory exceeds one page. Back and Close stay in the header throughout. A
reusable fixed-page footer supports these non-spell pickers without relying on
swipe or momentum behavior. All ten portable mobile
suites and the arm64 Simulator Release build pass. Simulator runtime review on
an iPhone 14 Pro covered portrait and landscape Party layouts, eight-spell
pages, page transitions, and both short and maximum-length spell details. A
follow-up Simulator pass visually verified the split Party row, the one-column
portrait spell list, and its two-column landscape reflow. A 12-weapon Simulator
fixture then verified landscape equipment pagination: the first page showed
four complete rows with `1–4 of 12`, and Next produced `5–8 of 12` without
clipping or scrolling.

### Interruption-safe checkpoints and recovery

iOS background transitions now produce at most one lifecycle checkpoint, stop
the gameplay timer, and reject keyboard, timer, semantic action, world-tap, and
combat-target input until the app is active again. Stable world and dungeon
states use the existing transactional generation save, advancing `CURRENT` and
retaining `PREVIOUS`; settlement, combat, and scripted states that the save
format cannot faithfully encode retain the latest safe checkpoint instead of
writing a partial state. Foregrounding restarts the timer and reports the
checkpoint result in recent activity without blocking play.

Journey preflight now validates the party, monster table, dungeon state, and
all present mobile metadata before accepting a checkpoint. If `CURRENT` is
missing or malformed and `PREVIOUS` is valid, the recovery sheet identifies
the slot, checkpoint time, map, dungeon level when applicable, and position
before offering the rollback. Portable tests cover duplicate lifecycle events,
inactive input policy, recovery routing, malformed pointers, retained
generations, pointer-only rollback, and a failed transactional write. All 12
portable mobile suites plus the arm64 Simulator and signed-device Release
builds pass.

Simulator runtime verification confirmed two background checkpoints advanced
`CURRENT` while retaining `PREVIOUS`, an open character-detail panel and its
selection survived normal suspension exactly, and a cold relaunch loaded the
new checkpoint. Corrupting only the `CURRENT` pointer produced the detailed
recovery sheet and successfully restored `PREVIOUS`; the newer fixture was
then restored so the Simulator remains on its latest playable checkpoint.

UIKit panel context is still process-local: it survives ordinary suspension,
but a terminated process resumes gameplay from the checkpoint rather than
reopening the prior native panel. Physical-device call, lock, termination, and
storage-exhaustion checks remain follow-up coverage; unsafe combat or scripted
states intentionally recover from the most recent representable checkpoint.

### Safe tap-to-walk routes

Ordinary iOS exploration now accepts taps across the visible 11-by-11 map and
uses a bounded breadth-first search to choose a route of at most 12 cardinal
steps. Planning uses the active transport's directional walk-on/walk-off rules,
current objects, map boundaries and wrapping, and excludes every terrain effect
tile. Visible NPC, door, and chest taps receive higher-priority contextual
approach behavior, and the D-pad remains the visible accessible alternative.

Only one original movement command is dispatched at a time, with a short delay
between turns. The next tile is revalidated immediately before it is entered,
so moving creatures and other dynamic blockers stop the route without spending
a blocked turn. Movement also stops on slowed terrain, map changes, encounters,
prompts, backgrounding, unsafe terrain, or any new keyboard or touch command.
Destination taps cancel old delayed steps before capturing their origin, so a
stale queued step cannot redirect or cancel a replacement route. Galloping
horses must slow first because their two-cell command cannot guarantee the
tapped destination.

Tap to walk is enabled by default on iOS and can be toggled independently under
Menu → Interaction controls. The touch guide and quality-of-life roadmap now
describe the shipped behavior. All 12 portable mobile suites pass, including
new pathfinding and settings-persistence coverage, and the arm64 iPhone
Simulator Release build succeeds. The final app was installed and launched on
the iPhone 14 Pro Simulator. Direct touch verification could not be completed
because the Mac UI was locked; route taps, cancellation timing, obstacle
detours, and encounter interruption remain runtime checks.

### Empty journey state

Choosing Journey onward without a saved adventure now opens a compact native
empty state with a real `Journey onward` heading, readable explanatory text,
and explicit New game and Title actions. New game continues into the normal
three-slot picker, while Title dismisses back to the opening menu. This replaces
the oversized blank sheet whose message was incorrectly parsed and truncated
as its navigation title. All 12 portable mobile suites and the arm64 Simulator
Release build pass.

### Approach-and-interact world taps

World taps now resolve visible friendly people, doors, and chests before
ordinary ground movement. The Avatar chooses the shortest safe action position
within the existing 12-step budget: adjacent to people and doors, on a chest,
or two tiles from a person when the intervening tile has the engine's talk-over
rule. The target and action are revalidated before execution, and wandering
people are replanned after each movement turn without extending the original
budget. Locked doors continue to report the explicit Unlock command and never
spend a key automatically.

Deliberate bump interaction uses the same talk-over rule, allowing shopkeepers
behind counter/sign tiles to be addressed without weakening wall collision.
All 12 portable mobile suites pass, including route-goal and approach-position
coverage, and the arm64 Simulator Release build succeeds.
