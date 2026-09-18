# Ultima IV Mobile Quality-of-Life Ideas

This document collects possible quality-of-life improvements for the mobile port of *Ultima IV*. The goal is to reduce bookkeeping, repetition, and touch friction without solving Britannia's mysteries for the player or changing the underlying game rules.

## Guiding principles

- Preserve the mechanics, information, timing, and player agency of *Ultima IV*.
- Reveal only information the player has encountered or deliberately recorded.
- Translate player intent to touch-native controls instead of reproducing keyboard commands literally.
- Keep the world visible when practical and place deep information in temporary sheets or overlays.
- Make assistance optional when it meaningfully changes the original experience.
- Support both portrait and landscape as intentional layouts.

## Highest-priority candidates

1. Expandable, exploration-based minimap with player pins.
2. Journal filters, search, favorites, and personal notes.
3. Spell castability explanations and visible reagent counts.
4. Discovery-gated conversation history and topic cross-references.
5. Context-sensitive world interaction.
6. Combat target cycling and range previews.
7. Exact-state resume and robust checkpoint recovery.
8. Control customization (D-pad sizing and Flip controls implemented; further customization planned).
9. Classic, Ultimatum, and Assisted Experience Profiles.
10. Optional companion battle tactics for reducing large-party turn management.

## Map and navigation

The first exploration-map release is implemented. Ultimatum and Assisted expose
a dedicated Map action; Classic leaves it off by default. The large temporary
map reveals only the ordinary 11-by-11 world view the player has actually seen,
keeps unknown terrain dark, marks the party's current or last overworld
position, supports party-centered button zoom and panning without gesture
recognizers, and persists its reveal state with the adventure checkpoints.
The player-pin follow-up is also implemented: Ultimatum and Assisted can add,
rename, and remove up to 24 short notes on explored overworld tiles. Pins are
adventure-slot metadata and join the next checkpoint without modifying the
underlying game state. Discovered-place markers are implemented as well:
successful entry records towns, castles, villages, shrines, and dungeon
entrances without scanning or revealing the untouched portal table. The
tap-to-expand local minimap is implemented in the overworld HUD. Dungeon-floor
exploration is implemented as well: each level records only tiles the party has
actually reached and reuses the tap-to-expand map affordance while inside a
dungeon. Dungeon exploration now has a dedicated native HUD with relative
Forward, Back, Turn Left, and Turn Right controls, direct Search and Torch
actions, contextual chest/ladder use, status and party information, and a
no-turn first-person/overhead toggle. The overhead renderer reuses the dungeon
tileset as a fixed north-up 8-by-8 board, draws only previously visited cells,
and switches the D-pad to cardinal movement. GEM/Peer maps now use the same native
map language for overworld, settlement, telescope, and dungeon views, with
visible zoom, pan, and Return controls. Debug Tools includes a non-consuming
native preview rather than switching the engine into its legacy DOS renderer.

- **Tap-to-expand minimap.** Implemented as a north-up, local explored-terrain preview in the overworld HUD; tapping it opens the full Britannia map.
- **Exploration-based reveal.** Show only terrain that the player has visited.
- **Player map notes.** Implemented as short, editable labels on explored overworld tiles, with an explicit Pin Here action and 44-point tap targets for existing pins.
- **Discovered-place markers.** Implemented for towns, shrines, castles, villages, and dungeon entrances after successful player entry, with read-only shape-and-color markers distinct from player pins. Place names appear beside visible markers by default and can be hidden with an explicit map control.
- **Dungeon controls and floor maps.** Implemented with a dedicated touch HUD,
  toggleable explored-cell overhead view, and per-dungeon/per-level maps that
  distinguish discovered ladders, rooms, fields, doors, and other features
  without revealing adjacent layout.
- **Location context.** Show the current region or location name without exposing exact coordinates unless an optional assist is enabled.
- **Optional breadcrumb trail.** Display the last several overworld moves, especially after sailing, balloon travel, or using a moongate.
- **Moon and wind panel.** Present the moon phases, wind direction, and transport heading clearly.
- **Discovery-gated moongate visualization.** Offer an optional cycle view only after the player has learned enough relevant information.

## Journal

- **Message filters.** Independently hide movement directions, combat rolls, routine confirmations, farewells, and other repetitive entries.
- **Search.** Implemented for collected dialogue, places, people, discovered topics, and attached/standalone personal notes.
- **Location-first conversation index.** Make locations the journal's first drill-down screen, followed by a separate list of encountered NPCs and records at the selected location. Within a location, give each NPC one continuous record in encounter order; do not split it into time- or visit-based groups. Paginate either list only when it exceeds the visible row capacity.
- **Broader clue collection.** Record encountered signs, books, visions, inscriptions, and other readable material.
- **Favorites and pins.** Implemented as passage-level bookmarks in "Current clues", with speaker/location/excerpt and return to the original continuous transcript. Removing a bookmark preserves the passage and any attached note.
- **Personal notes.** Implemented as separately labeled attached notes and standalone "My notes", with Done/Cancel, edit/delete confirmation, immediate per-slot persistence, and checkpoint recovery. Player writing never changes recorded words or teaches conversation keywords.
- **Topic navigation.** Tap a discovered keyword to see where it was learned or use it in the current conversation when valid.
- **Cross-references.** Link people, places, topics, and passages that the player has already encountered.
- **Unread indicators.** Mark newly recorded clues until the player views them.
- **Export.** Provide a plain-text share or export option for players who enjoy keeping external notes.

## Conversations

- **Recent topics.** Keep a short row of recently used keywords.
- **Discovery-gated suggestions.** Suggested topics must never expose secret keywords merely because they exist in NPC data.
- **Clear attribution.** Show the speaker and location at the top of the panel.
- **Current-session history.** Allow the player to scroll through the current exchange without opening the full journal.
- **Favorite topics.** Let the player pin frequently useful discovered topics.
- **Custom-keyword assistance.** Autocomplete only previously discovered words and retain a history of manually entered terms.
- **New-information treatment.** Subtly distinguish newly discovered names, places, or topics in a reply.
- **Consistent Goodbye action.** Keep it visible and in the same location across conversations.

## Movement and world interaction

The first contextual-action slice is implemented. A single exploration HUD
button now describes and performs an unambiguous action at the party's current
tile: Enter, Board, Dismount, Disembark, Ascend, Land, Climb, Descend, or Open
Chest. It is disabled rather than defaulting to Search when no useful action is
available. Talk and directional door commands remain visible explicit paths.
Bump and direct interaction share the same conservative resolver: pressing once
toward an eligible NPC starts Talk, including across a single tile carrying the
engine's talk-over rule, and an adjacent unlocked door opens. Collision-triggered interaction is specified in
[BUMP_INTERACTION_SPEC.md](BUMP_INTERACTION_SPEC.md): a blocked, deliberate move
may talk to the eligible NPC in that direction or open an unlocked door, while
explicit controls remain available and locked doors never spend keys
automatically.

- **Context-sensitive primary action.** Change between Talk, Open, Board, Descend, Enter, Search, or another appropriate verb when the intended action is unambiguous.
- **Bump to interact.** On a deliberate blocked movement attempt, talk to the
  eligible NPC in that direction—including across one engine-approved
  talk-over tile—or open an unlocked door under the safety and turn-accounting
  rules in [BUMP_INTERACTION_SPEC.md](BUMP_INTERACTION_SPEC.md).
- **Tap nearby NPCs to talk.** Implemented across the visible map: the Avatar follows a short safe route to an adjacent tile or a valid talk-over position, then starts the original conversation. The Talk button remains the reliable and accessible alternative.
- **Tap nearby objects to interact.** Implemented for visible doors and chests. The Avatar routes adjacent to a door or onto a chest before revalidating and performing the original action. Locked doors report the explicit Unlock path without spending a key.
- **Optional tap-to-walk.** Implemented for visible map tiles during ordinary exploration. Routes are bounded to 12 cardinal steps, use current collision and terrain rules, avoid hazardous tiles, and advance through the original movement command one turn at a time. Interactive taps choose the shortest valid action position, and a tapped wandering NPC is replanned within the original step budget. Routes stop for unresolved dynamic blockers, enemies, encounters, prompts, map changes, slowed movement, backgrounding, or any new player control; the D-pad remains available.
- **Adjustable movement repeat.** Let the player tune hold delay and repeat speed.
- **Flip controls.** Implemented under Menu → Controls: swap the gameplay D-pad and action buttons between left and right without moving the world viewport. Landscape status/party rails follow their controls to avoid overlaps; direction prompts stay opposite the D-pad.
- **Control customization.** D-pad sizing is implemented with Small, Standard, and Large choices. Sizes fit available safe-area space while retaining at least 44-point direction targets. Sizing and Flip controls persist across adventures independently of Experience Profiles. Adjustable opacity, spacing, and free placement remain planned.
- **Avoid accidental wasted turns.** Disable or explain commands that cannot do anything in the current context while retaining a deliberate Wait action.
- **Clear transport state.** Make on-foot, horseback, ship, and balloon states immediately recognizable.

## Party and inventory

- **Baseline party status.** The persistent party HP and condition roster is
  part of the Core Mobile Standard, not a quality-of-life feature. It is
  specified in [EXPERIENCE_PROFILES_SPEC.md](EXPERIENCE_PROFILES_SPEC.md) and
  remains present under every Experience Profile.
- **Tap-through details.** Open a party member's details by tapping their roster entry.
- **Equipment comparison.** Preview attack or defence changes before confirming equipment.
- **Relevant choices only.** Offer only equipment that is owned and usable by the selected character.
- **Consistent spare counts.** Display spare inventory quantities in equipment, vendor, and party views.
- **Quick transfer.** Simplify moving usable items between party members.
- **Contextual common items.** Surface torches, gems, keys, sextants, and quest items when relevant.
- **Rare-item protection.** Confirm dropping or consuming important quest items.
- **Remember selection.** Reopen the party panel on the last-viewed member.

## Magic and reagents

- **Spell names beside letters.** Retain the classic letter while making the full spell name immediately readable.
- **Distinct spellbook navigation.** Keep spell choices visually separate from a fixed Previous/Next footer, page-range indicator, and dismissal control.
- **Castability explanations.** Explain whether a spell is unavailable because of MP, reagents, location, combat restrictions, caster, or recipient.
- **Castable-spell filter.** Optionally hide or dim spells that cannot currently be cast.
- **Favorite spells.** Provide a compact favorite-spell row.
- **Remember safe selections.** Retain the last caster and recipient where doing so cannot cause a surprising action.
- **Visible reagent stock.** Show current reagent quantities directly in the spellbook and mixing interfaces.
- **Batch mixing.** Mix several copies of a spell at once.
- **Known-recipe notebook.** Record mixtures the player has successfully made without revealing undiscovered recipes.
- **Quick recast.** Repeat the previous valid spell after clearly confirming its caster, recipient, and cost.

## Combat

The first targeting pass is implemented. Attackable combatants receive generous
touch regions, the HUD exposes nearest-ordered Previous/Next Target and Clear
controls, and the selected combatant and attack path use shape plus color. Target
selection is a no-turn preparation step; the explicit Attack action commits the
original weapon path, hit, damage, ammunition, tile-effect, and turn rules.

- **Tap-to-target.** Implemented with generous hit regions around combatants that are valid for the active weapon.
- **Target cycling.** Implemented with next and previous valid-target controls ordered by distance.
- **Clear selection.** Implemented with active-party and current-target state that does not rely on color alone.
- **Attack range preview.** Implemented as a selected-target path preview before the explicit Attack confirmation.
- **Movement preview.** Show the destination square before accepting a combat movement tap.
- **Repeat last action.** Repeat-last-attack is implemented: the D-pad's center Repeat Attack prepares each fighter's last reachable enemy with the same weapon; the named Attack button commits and Clear cancels without a turn. Spell repetition remains a candidate.
- **Contextual chest action.** Surface the appropriate chest command when a character is adjacent to one.
- **Combat speed options.** Standard/Fast pacing is implemented under Controls, including the battle pause menu. Fast shortens longer combat flashes and round pauses without changing turn rules.
- **Previously read message skipping.** Fast pacing omits the repeated turn-start fighter/weapon banner while retaining all outcome messages. Broader narration filtering remains a candidate.
- **Flee confirmation.** Protect against an accidental retreat without adding friction to ordinary movement.

### Companion battle tactics

Large parties make *Ultima IV* combat repetitive because the player may need to
issue commands for as many as eight characters every round. Add an optional
companion automation system that lets the player assign a combat tactic to each
party member while retaining manual control whenever desired.

This idea has a direct series precedent. *Ultima VI* allowed each character to
use Command or an automatic combat behavior, including formation-oriented,
aggressive, and retreating modes. Its original reference guide also allowed a
character's mode to be changed during battle. Ultimatum should borrow the
player-configured intent, not reproduce the original AI literally. See the
[Ultima VI Reference Guide](https://mocagh.org/origin/u6wc-u6-refcard.pdf).

Recommended initial tactics:

- **Manual.** Stop and wait for the player on this character's turn.
- **Assault.** Move toward and attack the nearest safely reachable hostile.
- **Hold position.** Attack a hostile already in range; otherwise pass.
- **Ranged.** Prefer targets reachable with the equipped ranged weapon and
  avoid unnecessary advance.
- **Protect.** Remain near the Avatar or a selected ally and engage nearby
  threats.
- **Retreat.** Move toward a valid exit or away from immediate threats without
  attacking unless blocked and threatened.

The first implementation should keep the Avatar manual by default. It should
also avoid automatic spellcasting, quest-item use, consumable use, equipment
changes, friendly fire, attacks on non-hostile targets, and weapon actions that
can unexpectedly sacrifice a scarce item. Those behaviors require separate,
explicit policies if introduced later.

Automation must execute one ordinary legal action on the acting character's
ordinary turn. It receives no accuracy, damage, movement, initiative, resource,
or targeting advantage. The existing combat rules, random-number use, tile
effects, food consumption, victory checks, and enemy turn remain unchanged.

The combat HUD should clearly mark automated characters and their tactic. A
persistent **Take Control** or **Stop Auto** action must halt automation before
the next companion action. The player may change tactics during combat, with
changes taking effect at the next safe character-turn boundary. A whole-party
Auto command may temporarily use the saved tactics for all eligible companions;
it must be visibly active and immediately cancellable.

Tactics belong to the adventure because they refer to its current party
members. Turning the feature off must retain those assignments so they return
if the player enables it later. Newly recruited companions default to Manual
until the player chooses otherwise.

## Saving and mobile lifecycle

- **Multiple adventure slots.** Three independent save slots are part of the
  Core Mobile Standard and remain available under Classic, Ultimatum, and
  Assisted. Each slot owns its party, journal, explored map, automatic
  checkpoints, and previous-checkpoint recovery history. Existing adventures
  remain in Slot 1 without migration.
- **Interruption-safe checkpoints.** Save frequently enough that a call or app suspension is not catastrophic.
- **Exact-state resume.** Restore the open panel, selected character, journal position, targeting state, and orientation where safe.
- **Non-blocking save confirmation.** Confirm successful saves without interrupting play.
- **Previous-checkpoint recovery.** Show the checkpoint's timestamp and location before restoring it.
- **New-game protection.** Warn before replacing existing progress.
- **Safe background pause.** Prevent elapsed time or stray input from changing the game while the app is inactive.
- **Optional iCloud synchronization.** Resolve conflicts explicitly instead of silently overwriting newer progress.

## Accessibility and comfort

- **Scalable pause navigation.** Keep Resume as a fixed header action. If the
  pause menu outgrows its compact grid again, promote related actions into
  labeled native category pages instead of shrinking touch targets or making
  the compact game overlay scroll.
- **Dynamic text size.** Apply text scaling consistently throughout native panels and the HUD.
- **High-contrast and reading options.** Include a high-contrast treatment and an optional more-readable typeface.
- **Color-independent state.** Pair color with icons, labels, shapes, or patterns for conditions and targeting.
- **Reduced motion and flashing.** Offer calmer transitions and effects.
- **Separate audio controls.** Provide independent music, effect, and interface volume.
- **Optional haptics.** Use meaningful feedback for movement obstruction, successful interaction, damage, and important confirmation.
- **VoiceOver support.** Give controls useful labels, hints, values, and a logical navigation order.
- **Controller support.** Map controller input to the same semantic actions available through touch.
- **Per-orientation preferences.** Remember portrait and landscape control arrangements separately.
- **One-handed portrait option.** Keep the most frequent actions within one-thumb reach.

## Optional assistance

These features should be opt-in because they alter the original experience more noticeably.

- **Experience Profiles.** Offer Classic, Ultimatum, and Assisted defaults, with individual settings still adjustable.
- **Map coordinates.** Show exact coordinates on the large map.
- **Non-spoiler reminders.** Summarize only goals and clues the player has already encountered.
- **Virtue history.** Record witnessed virtue gains or losses without displaying hidden numerical values.
- **Reagent shopping list.** Build a list from spells the player marks as favorites.
- **Vendor comparison.** Show affordability, owned quantities, and equipment comparisons.
- **Lasting-choice confirmation.** Optionally confirm choices with major consequences without revealing which response is "correct."
- **Failure-sensitive hints.** Offer contextual help after repeated failed attempts without revealing undiscovered answers or destinations.

## Authenticity boundary

Quality-of-life features should reduce interface labor, not intellectual discovery. In particular, they should not:

- Reveal conversation topics the player has not encountered.
- Reveal map locations, recipes, runes, mantras, answers, or quest solutions prematurely.
- Display hidden virtue scores by default.
- Automate choices with moral or strategic consequences.
- Change combat probabilities, resource costs, turn accounting, or world rules unless introduced as an explicitly separate gameplay option.

The working rule is: **reduce bookkeeping, repetition, and touch friction, but never solve Britannia's mysteries on the player's behalf.**
