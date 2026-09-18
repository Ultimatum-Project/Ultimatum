# Bump Interaction Specification

Status: Implemented (iOS); portable and build verification complete
Applies initially to: *Ultima IV Mobile*

## Purpose

Bump interaction lets a deliberate movement attempt express the obvious action
when an adjacent target blocks the Avatar. Its primary case is walking into a
conversable NPC to begin talking. It reduces repeated `Talk` → direction input
without changing dialogue, collision, turn, or world-simulation rules.

This is a shortcut, not the only interaction path. The visible Talk and
contextual Interact controls remain available in every supported orientation,
for VoiceOver, and with a controller.

## Definition and scope

A **bump** is a user-initiated cardinal movement attempt from normal exploration
into the immediately adjacent tile when ordinary movement resolution returns
blocked. Version 2 resolves only these unambiguous targets:

1. **Conversable, non-hostile person:** begin the same conversation as explicit
   Talk in that direction. The person may be adjacent, or exactly two tiles away
   when the intervening tile carries the engine's `talkover` rule.
2. **Unlocked door:** perform the same Open action at that coordinate.

Except for the explicit one-tile talk-over case, the target must be on the
attempted destination tile. Bump interaction does not search farther down a
direction, choose among multiple coordinates, or infer a target from visual
proximity.

## Safety and exclusions

- Run only during ordinary exploration. Never run in combat, dungeon
  first-person movement, menus, conversations, targeting modes, cutscenes, or
  while another native panel owns input.
- Require an actual blocked movement result. Successful movement, slowed
  movement, map exits, transport turning, balloon drift, and other movement
  results retain their existing behavior.
- Talk only to a person for whom the existing engine conversation eligibility
  succeeds. Hostile or alerted guards, non-conversable actors, creatures,
  hazards, and world-map encounters retain existing collision/combat behavior.
- Do not automatically spend a finite resource. In particular, bumping a
  locked door must not consume a key. It remains blocked, reports that it is
  locked, and points to the explicit `Explore` → `Unlock` action.
- Do not trigger current-tile actions such as Enter, Board, Dismount,
  Disembark, Climb, Descend, Open Chest, Ascend, or Land. Those remain the
  responsibility of the contextual Interact button.
- One physical press may trigger at most one bump interaction. Directional
  auto-repeat must not repeatedly reopen a conversation or invoke Open; the
  trigger rearms after release or after a different successful movement.
- Collision override and developer cheats bypass bump resolution.

## Resolution order

After ordinary movement reports blocked for a user event:

1. Inspect the exact attempted destination.
2. If it contains an eligible person, resolve Talk.
3. Otherwise, if its effective tile is an unlocked door, resolve Open.
4. Otherwise, if it carries the engine's talk-over rule, inspect exactly one
   additional tile in the attempted direction and resolve an eligible person.
5. Otherwise retain the original blocked result and feedback.

Person takes priority over terrain so an eligible actor standing in a doorway
cannot cause the door action to bypass the actor.

## Turn and state semantics

- A successful bump action replaces the failed movement attempt; it must not
  charge both a blocked-movement turn and a command turn.
- The resulting Talk or Open operation uses the same turn accounting, scripts,
  virtue consequences, journal capture, sound, and save behavior as its
  explicit command path.
- The Avatar remains on the origin tile unless the reused engine action itself
  changes location under existing rules.
- If eligibility changes between collision detection and action execution,
  fail safely as an ordinary blocked move. Never redirect to another target.
- Cancelling or ending a bump-started conversation behaves exactly like an
  explicitly started conversation.

## Feedback

- Suppress the redundant `Blocked!` message and blocked sound when Talk or Open
  succeeds.
- Replace generic blocked feedback with `Locked door` and explicit Unlock
  guidance when the attempted destination is a locked door.
- Conversation presentation is sufficient confirmation for Talk. Door
  animation/sound is sufficient confirmation for Open.
- A light haptic may accompany a successful bump interaction when haptics are
  enabled; ordinary blocked movement must remain distinguishable.
- No persistent target highlight or mode is introduced.

## Settings and profiles

`Bump interactions` is a Controls preference and defaults on for iOS. It is a
touch/controller input translation covered by Contextual touch actions, which
is enabled in Classic, Ultimatum, and Assisted; changing it does not mark an
Experience Profile as customized. Desktop behavior remains unchanged.

The implementation should use a dedicated setting rather than silently
expanding the legacy `shortcutCommands` flag, whose existing door/key behavior
is broader than this specification.

## Implementation boundary

The integration point is blocked user movement handling in
`GameController::avatarMoved`, after the engine has identified the attempted
coordinate and before blocked feedback is emitted. Target resolution should be
a side-effect-free helper; execution should call the existing `talkAt`,
`openAt`, and conversation paths rather than duplicate their rules.

The movement result needs an explicit representation for “blocked movement was
replaced by an interaction” so end-of-turn handling cannot accidentally charge
twice. Do not represent a conversation as `MOVE_SUCCEEDED`, because the Avatar
did not enter the destination tile.

## Acceptance criteria

- Bumping an eligible NPC once—including across one talk-over counter/sign—opens
  that NPC's conversation and does not move the Avatar.
- Bumping the same NPC via held D-pad input opens only one conversation.
- Bumping a non-conversable or hostile person preserves normal blocked behavior.
- Bumping an unlocked door opens that exact door once.
- Bumping a locked door consumes no key, preserves blocked-position and turn
  behavior, and reports how to reach the explicit Unlock command.
- Bumping a wall, chest, vehicle, ladder, field, or empty impassable tile does
  not infer another command.
- Explicit Talk and Interact remain usable and semantically unchanged.
- Movement and action turn counts match the corresponding original commands;
  no bump case advances two turns.
- Touch, keyboard, and controller-originated user movement share the same
  resolver when the preference is enabled; NPC autonomous movement never does.
- Portrait/landscape rotation and app interruption during a bump-started
  conversation preserve the same state as explicit Talk.

## Verification plan

Add portable resolver and turn-accounting tests for every acceptance case.
Simulator testing must cover a fixed NPC, a shopkeeper across a talk-over tile,
a wandering NPC, alerted guard, unlocked door, locked door with keys in
inventory, held D-pad repeat, explicit Talk fallback, rotation during
conversation, save/relaunch after dialogue, and VoiceOver access to the visible
alternative controls.
