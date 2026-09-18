# Mobile dungeon gameplay specification

## Purpose

Make every ordinary Ultima IV dungeon action playable with touch while keeping
the dungeon viewport visible, preserving the original turn rules, and avoiding
keyboard-only or gesture-only commands. The dungeon HUD is core mobile
playability, not an optional quality-of-life feature.

This specification follows `PC_to_iOS_Interface_Playbook.md`: immediate movement
and dungeon actions stay thumb-reachable, the viewport remains central, deeper
systems use temporary panels, touch targets remain at least 44 points, and
portrait and landscape use composed layouts rather than scaled desktop chrome.

## Supported states

The native dungeon HUD owns ordinary `VIEW_DUNGEON` exploration while the main
game controller owns input. Dungeon rooms continue into the existing combat HUD.
Native sheets continue to cover their controls while open. Special sequences
that still require their own semantic panel may temporarily use the legacy
fallback, but must never be mislabeled as an introduction or expose a Skip
button.

## Persistent HUD

- Keep the 176-by-176 dungeon viewport in the same square map allocation used
  by exploration and combat.
- Show dungeon name, one-based level, facing direction, lit/dark state, remaining
  torch duration, carried torches, food, and gold.
- Keep the compact party health/condition roster and tap-through party details.
- Show the discovery-gated dungeon-level minimap in the top utility position.
- Present the expanded floor map as a bounded, north-up 8-by-8 grid so unknown
  cells remain visibly part of the floor rather than blending into the panel.
- Keep the recent activity log over the lower edge of the viewport without
  blocking its centre.

## Movement

The D-pad describes dungeon-relative intent instead of cardinal directions:

- Forward: advance one cell in the current facing direction.
- Back: retreat one cell without changing facing.
- Turn Left / Turn Right: rotate in place.

Forward and Back may repeat on hold. Turns are one-shot so a held thumb cannot
spin through several orientations. These controls dispatch through the existing
movement engine and retain its collision, trap, encounter, food, torch, and turn
accounting.

## Immediate actions

- Search: run the original dungeon Search command. This may discover an orb,
  fountain, altar item, or other current-cell content and uses the original turn
  and downstream party-selection behavior.
- Torch: light one carried torch using the original inventory and duration rules.
- Context action: show Open Chest, Climb, or Descend only when valid at the
  current cell; otherwise remain visibly disabled.
- Spells, Party, Menu, and Wait retain the same semantic paths as exploration.
- Journal is available from the pause menu while the permanent dungeon action
  slot is used for Torch.
- No critical dungeon action depends on a swipe, long press, hidden tap region,
  software keyboard, or attached controller.

## Overhead-view stretch goal

A visible button toggles between the original first-person presentation and an
overhead presentation built from the existing dungeon tileset. The toggle is
presentation-only, costs no turn, and defaults to first-person when the renderer
is first created.

The overhead view is a fixed north-up rendering of the complete 8-by-8 floor.
It never repeats cells to fill the 11-by-11 classic viewport. The party moves
within that fixed board, and the D-pad changes to cardinal North, South, West,
and East movement. A cardinal move faces the party in that direction and then
uses the ordinary forward movement path; this preserves collision, wrapping,
hazard, encounter, and turn rules while translating touch intent directly.

Previously seen cells remain rendered. The live view reveals and remembers the
party's current cardinal line of sight along each hallway, stopping at the
first wall, secret door, ordinary door, or room entrance. Because dungeon floors
wrap, sight crossing an edge continues at the mechanically adjacent cell on the
opposite edge; that cell remains drawn only once in its fixed map position.
The blocking cell itself is remembered, but cells beyond it remain black unless
seen from another legitimate vantage point. A remembered secret door retains
ordinary wall presentation so the map cannot disclose its identity. The status
line and accessible button value identify the current facing and presentation
mode. Returning to first-person restores the relative Forward, Back, Turn Left,
and Turn Right controls.

## Accessibility and feedback

- Each D-pad direction has a semantic label and hint in dungeon mode.
- The view toggle states both the destination action and the current mode.
- Disabled contextual actions remain visually distinct.
- Lit/dark state is communicated with text, not color alone.
- Both layouts retain safe-area clearance, 44-point minimum targets, Dynamic
  Type status text, and the existing VoiceOver party navigation order.

## Acceptance criteria

1. Entering any dungeon replaces the legacy full framebuffer with the native
   viewport, status, party roster, minimap, D-pad, and dungeon actions.
2. The introduction-only Skip control never appears during dungeon gameplay.
3. Forward, Back, Turn Left, and Turn Right produce the same engine results as
   their keyboard equivalents; only Forward and Back repeat on hold.
4. Search, Torch, context action, Spells, Party, Menu, Journal-from-menu, and
   Wait all have visible touch paths.
5. Overhead view toggles without spending a turn, remains north-up, renders
   each floor cell at most once, uses cardinal movement, and shows explored
   remembered cells plus current hallway line of sight without revealing
   through blockers or identifying secret doors. Sight and movement agree at
   wrapping floor seams.
6. Rotation retains the dungeon, facing, view mode, and active native panel.
7. Dungeon room combat enters and leaves the combat HUD without exposing legacy
   controls.
8. Simulator and device Release targets compile, portable tests pass, and a
   runtime pass covers both orientations, light/dark state, a ladder/chest, the
   view toggle, room combat, and returning to Britannia.
