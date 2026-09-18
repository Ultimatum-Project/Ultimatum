# Ultima IV on iOS — Touch Controls

The touch HUD supports both portrait and landscape. The classic viewport stays
square and central while status, party information, movement, and semantic
actions recompose around it.

**Menu → Controls** offers **D-pad size** (Small, Standard, Large) and
**Flip controls** (swap movement/actions between left and right). Preferences
apply immediately and persist across adventures. Larger sizes are fitted to
available space without moving or covering the world; Small still has
44-point direction targets. Landscape party/status rails follow their controls,
and direction prompts remain opposite the D-pad. These preferences are separate
from Experience Profiles; restoring a profile does not reset them.

Bump, tap-to-interact, and tap-to-walk toggles are under
**Menu → Controls → Touch interactions**.

**Menu → Adventure backups** imports a complete `.u4save` adventure through
Files and exports one through the iOS share sheet (including **Save to Files**).
The same backups work on the web client. **Journey onward / New adventure →
Import / Export adventures** also exposes backups before loading a slot.
They include saved game progress, journal, bookmarks, personal notes, explored
maps, pins, discoveries, and any imported web conversation history—not game
data or device preferences. Import validates every file before publication,
asks before replacing an occupied slot, and retains its previous checkpoint
for recovery. During play, the active slot cannot be replaced. Export can save
current progress in a safe context or explicitly use the last checkpoint.
Cancelling Files/sharing does not replace a slot or spend a turn. Cloud sync
and Ultimatum Accounts are not part of this feature.

**Menu → Controls → Combat pacing** offers **Standard** (the default) and
**Fast**. Fast shortens longer combat flashes and round pauses, and omits the
repeated fighter/weapon turn-start banner already shown by the HUD. Hit, miss,
damage, condition, death, and other result messages remain. This is a persistent
device preference independent of Experience Profiles and does not change turns,
attack rules, or costs. Controls is also available from the battle pause menu.

| Action | Touch |
|---|---|
| **Move / bump interact** | On-screen **D-pad**; press once toward a friendly person, including across an engine-approved counter/sign, or an unlocked door |
| **Tap to walk** | Tap a visible map tile during ordinary exploration to follow a short safe route; any other control or destination tap cancels it |
| **Direct interaction** | Tap a visible friendly person, door, or chest; with Tap to walk enabled, the Avatar approaches the correct interaction tile and then acts |
| **Commands** | Use the visible semantic actions and pause-menu categories; tap **⌨** when a legacy prompt needs text input |
| **Show/hide keyboard** | The **⌨** button (bottom-right) |
| **Context action** | Tap the changing **Interact / Enter / Board / Climb / Descend / Open Chest** button when available |
| **Confirm / advance text** | **Continue** button, or Return on the keyboard |
| **Wait / pass turn** | **Wait** button during gameplay |
| **Back out of a legacy menu** | **Esc** button when shown |

Both interaction modes and **Tap to walk** can be toggled independently under
**Menu → Controls → Touch interactions**. Holding a D-pad direction only repeats movement,
never conversation or door actions. Locked doors still use **Explore → Unlock a
door**; tapping or bumping one reports that guidance without spending a key. The
visible **Talk** and contextual **Interact** buttons remain explicit fallbacks.

Tap routes are limited to visible terrain and at most 12 cardinal steps. They
avoid hazardous tiles and stop for dynamic blockers, enemies, encounters,
prompts, map changes, slowed movement, backgrounding, or a new player control.
Galloping horses must slow first. The D-pad remains the visible and accessible
movement path.

## Combat

Tap a combatant or use **Previous/Next Target**, then **Attack** to commit.
The Attack button names the selected enemy and the world highlights the path.
**Repeat Attack**, in the empty center of the D-pad, prepares the active fighter's
last attack target without spending a turn. **Attack** confirms it; **Clear**
cancels. Each fighter remembers their own target only for the current battle.
Repeat is unavailable if that enemy is gone, blocked, out of range, or the
fighter's weapon changed. Moving enemies are rechecked at their current position;
a different enemy standing on the old tile is never substituted. Invalidating a
prepared repeat cannot fall back to a blind directional attack. Consumable and
returning weapons still use their original rules and costs on confirmation.
Repeat does not cast spells, automate companions, or advance turns on its own.

The mobile UI translates the common single-letter commands into labeled touch
actions. Conversations still accept typed keywords such as `name`, `job`,
`health`, and `bye` through their native conversation panel.

## Dungeons

Entering a dungeon automatically switches the HUD to dungeon-relative controls.

| Action | Touch |
|---|---|
| **Advance / retreat** | **Forward** and **Back**; these may repeat while held |
| **Change facing** | **Turn Left** and **Turn Right**; each tap turns once |
| **Search the current cell** | **Search** |
| **Light a torch** | **Torch** |
| **Use the current tile** | The changing **Climb / Descend / Open Chest** action when available |
| **Switch presentation** | **Overhead** or **3D View**; this costs no turn |
| **Open the explored floor map** | Tap the small dungeon minimap |

Overhead mode uses the original dungeon tileset as a fixed north-up 8×8 floor
and draws only cells the party has already visited. Its D-pad becomes cardinal
North, South, West, and East movement; returning to 3D restores the relative
controls. Journal is available from **Menu** while the permanent dungeon action
slot is used for Torch.

**Menu → Journal** opens a full-screen, paused notebook. **Places** remains
the default location/person index; each person's words stay in one continuous
transcript. The bookmark icon in a passage's upper-right corner adds it to **Current clues**;
selecting the clue returns to that passage in context. Unbookmarking never
removes recorded words or an attached note.

The paper-and-pencil icon attaches or edits separately labeled player writing to a
passage. **My notes → New note** creates a standalone reminder. **Done** saves
immediately in the active adventure slot; **Cancel** discards the draft.
Deleting an existing note requires confirmation and preserves its source and
bookmark. Search includes personal writing, but notes never teach dialogue
keywords. Lists and transcripts use visible Previous/Next controls rather
than requiring drag scrolling. Checkpoints and recovery carry the matching
notebook; older adventures start with an empty notebook. The three view
selectors use plain buttons and preserve selection with a highlighted background
and accessibility selected state.

Passage action icons retain 44×44-point tap areas. A filled bookmark indicates
an active clue; a small check badge on the note icon indicates an attached note.
VoiceOver names the actions and their saved states. The original dialogue and
the full-screen journal layout are unchanged.

When the keyboard is up, the game scales to stay fully visible above it, and the
buttons stay put; empty screen areas pass touches through to the game.

Journey Onward and New Game provide three independent adventure slots. Existing
adventures appear in Slot 1. The pause menu names the active slot when saving,
and periodic checkpoints stay isolated within that slot.

Local development and preview builds also expose **Menu → Debug Tools**. The native category pages
provide session switches, navigation, party and inventory setup, world actions,
diagnostics, and explicitly marked destructive commands. The first action that
changes an adventure offers to create a recovery checkpoint. **Resume** remains
fixed in the pause-menu header so adding Debug Tools does not shrink the ordinary
touch targets.
