
# PC → iOS Interface Playbook
A reusable design doctrine for translating desktop games into excellent touch-first iPhone and iPad experiences

Core idea: preserve the game, not the desktop interface. Translate player intent—not pixels, windows, and keyboard bindings.

Reference edition • September 2026

## 1. The doctrine in one page
Use these defaults unless the game gives you a strong reason not to. They are deliberately opinionated so a team—or coding agent—can make consistent choices without re-litigating basic mobile UX on every screen.
1. Preserve game semantics, not desktop chrome. Keep mechanics, information, timing, and agency. Redesign the way players express those things on touch.
2. Make thumbs the primary pointing device. Frequent actions belong in comfortable lower-screen zones; persistent status can live higher.
3. Replace precision with intent. Use snapping, target cycling, contextual actions, nearest-target commands, generous hit regions, and smart defaults.
4. Keep world interaction and command interaction legible. A tap on the world should have a predictable meaning; explicit commands should use explicit controls.
5. Progressive disclosure beats permanent windows. Show only what matters now. Put deep systems in sheets, drawers, overlays, and temporary panels.
6. Active-play actions must be one step away. Combat, healing, targeting, pause, and other time-sensitive actions should not require menu spelunking.
7. Touch controls should be customizable. Support scale, placement, opacity, handedness, hotbar composition, and gesture sensitivity where practical.
8. Portrait and landscape are separate layouts. Treat orientation as an information-architecture decision, not a rotation event.
9. Never depend on hover or desktop-only states. Translate hover, right-click, modifier keys, tooltips, and drag-and-drop into explicit touch patterns.
10. Controller support complements touch. Controller support is valuable, but it is not permission to ship poor touch UX.
11. Design for interruption. Mobile play needs robust pause/suspend, autosave, state restoration, and short-session tolerance.
12. Accessibility is part of the control model. Large targets, readable text, remapping, color-independent feedback, haptics, and reduced gesture dependence improve the product for everyone.
### Default interaction targets

## 2. Start with player intent
The worst ports begin with the desktop screenshot and ask, “How do we fit this on a phone?” Better ports begin with a list of player intents and ask, “What is the fastest, clearest touch interaction for each one?”
### Build an intent inventory before designing controls
- Navigate or move through the world.
- Orient the camera or inspect surroundings.
- Select, target, or focus an entity.
- Execute an attack or ability.
- Interact with an object or NPC.
- Loot, equip, consume, craft, or reorganize items.
- Read status and make strategic decisions.
- Communicate, type, search, or enter names.
- Open deep information such as skills, quests, maps, logs, or configuration.
Then rank each intent by frequency, urgency, precision requirement, and whether it occurs during active play. Frequent + urgent intents deserve permanent thumb-accessible controls. Rare + non-urgent intents can live in secondary UI.
### A useful translation rule
Desktop input describes a mechanism (“press Tab,” “right-click,” “drag this icon”). Mobile design should describe the intent (“cycle targets,” “inspect,” “move this item”).
## 3. Thumb-first screen architecture
On a phone, reachability is part of the game’s ergonomics. Build layouts around what players must hit repeatedly, not merely around visual symmetry.
- Lower-left: movement controls, virtual stick, movement affordances, or secondary thumb actions.
- Lower-right: attack, interact, target, heal, ability cluster, or other high-frequency commands.
- Top corners: minimap, health/status summaries, objectives, connection state, or other glanceable information.
- Center: protect the world viewport and the player character from permanent chrome.
- Edges: temporary drawers, tabs, contextual trays, and low-frequency controls.
Do not assume every player holds the device the same way. Offer left-handed or mirrored layouts when the game relies heavily on persistent controls.
### Reachability hierarchy
- Tier A — immediate: attack, movement, dodge, heal, target, pause.
- Tier B — frequent: inventory shortcut, map, spell/ability selection, party control.
- Tier C — occasional: character sheet, quest log, crafting, settings.
- Tier D — rare: account management, advanced configuration, debug or social administration.
## 4. Desktop → touch translation patterns
Do not emulate the mouse and keyboard literally. Translate each desktop convention into the most legible touch-native pattern that preserves intent.
### Contextual actions
When the game can safely infer intent, collapse several desktop verbs into one context-sensitive action. “Open / talk / loot / use” can often become one Interact button whose label or icon changes with focus. The rule is predictability: inference should reduce work without surprising the player.
### Touch modes
- Prefer momentary modes: the player holds or taps a visible control, performs an action, and returns to normal.
- If a persistent mode is necessary, make it visually unmistakable and easy to cancel.
- Do not overload a plain world tap with too many hidden mode-dependent meanings.
- When uncertainty is high, favor selection first and explicit action second.
## 5. Targeting and combat
Combat is where desktop ports most obviously fail because keyboard shortcuts and mouse precision disappear at the same time. Mobile combat needs an explicit targeting model.
### Recommended targeting stack
- Tap an entity to select it when precision is reasonable.
- Expand interactive hit regions beyond visible sprite/model bounds.
- Provide Target Nearest / Target Closest Hostile when useful.
- Provide cycle-next / cycle-previous for crowded scenes or accessibility.
- Keep a clear current-target indicator in both the world and HUD.
- Allow abilities to use the current target when appropriate.
- For ground targeting, snap or magnify the target point and make cancel obvious.
- For repeated actions, consider Attack Last Target / Use Last Target style commands.
### Action cluster design
A strong default for action-heavy games is a configurable cluster near the lower-right thumb: one large primary action plus 3–6 smaller high-frequency actions. Secondary abilities can live in a swipeable hotbar, radial menu, or temporary expanded tray.
## 6. HUD, windows, and progressive disclosure
Desktop games often spend screen area freely because monitors are large and windows can coexist. On iPhone, every persistent panel competes with the world viewport.
### HUD rule: permanent only if glanceable and continuously useful
- Health/resources/status that influence immediate decisions.
- Current target or current objective when relevant.
- Minimap or navigation cue if spatial awareness is core gameplay.
- A small set of immediate commands.
- Critical warnings and transient feedback.
Inventory, skill trees, journals, crafting, settings, detailed statistics, and historical logs usually belong in temporary UI—not permanent chrome.
### Panel patterns
- Bottom sheet: excellent for item details, conversations, contextual commands, and short lists.
- Edge drawer: useful for logs, party panels, secondary hotbars, or navigation.
- Full-screen modal: appropriate for deep systems that pause or safely suspend gameplay.
- Partial overlay: useful when world context still matters, such as inventory during looting.
- Tabbed hub: useful when several deep systems share navigation, e.g., character / skills / equipment / quests.
### Avoid “desktop window cosplay”
Tiny draggable windows, miniature title bars, nested scroll regions, and pixel-perfect inventory grids can feel faithful while being physically unpleasant. Keep the visual identity if desired, but rebuild the interaction geometry for fingers.
## 7. Inventory and item manipulation
Inventory is a special case because many PC games make mouse dragging part of their identity. Preserve satisfying direct manipulation where it matters, but do not make it the only reliable way to perform routine actions.
- Increase item hit areas independently of art size.
- Tap once to select and expose common actions such as Use, Equip, Split, Move, Drop, Inspect.
- Allow drag-and-drop as a fast optional shortcut when targets are large enough.
- Use auto-stack, auto-sort, auto-place, and quantity defaults to eliminate repetitive precision work.
- For grid inventories, support zooming or enlarged item cells rather than reproducing a tiny desktop grid.
- Use long-press for secondary item actions only when it does not conflict with scrolling or drag initiation.
## 8. Portrait vs. landscape
Orientation changes the information architecture. Treat each supported orientation as a composed layout with its own priorities.
If both orientations are supported, share the interaction vocabulary but permit different placement and disclosure. Do not force identical geometry.
## 9. Gestures: powerful shortcuts, poor secrets
- Use familiar gestures for familiar meanings: pinch to zoom, drag to pan, swipe to scroll or change pages.
- Do not make an undiscoverable gesture the only way to perform a critical action.
- Provide visible alternatives for important gesture commands.
- Avoid gesture collisions: scrolling, dragging items, camera panning, and long-press cannot all compete on the same surface without careful state design.
- Use onboarding or lightweight hints when a gesture materially improves play.
## 10. Text entry and communication
The software keyboard is disruptive because it consumes a large portion of the screen. Design text workflows to be short and intentional.
- Prefill sensible defaults and remember recent entries.
- Use autocomplete, saved phrases, command suggestions, recipient chips, or history where applicable.
- Keep the input field visible when the keyboard appears.
- Pause or protect the player during mandatory typing when the game design permits.
- For chat-heavy PC games, support external keyboards/controllers but keep touch entry viable.
## 11. Feedback: make touch feel trustworthy
- Immediate visual state change when a control is pressed.
- Haptic feedback for meaningful confirmation, not every trivial touch.
- Clear selected-target, selected-item, mode, cooldown, and disabled states.
- Forgiving cancellation for destructive or high-cost actions.
- Do not rely on color alone to indicate important states.
## 12. Accessibility, customization, and controllers
- Adjustable UI scale and text scale where the game architecture permits.
- Control remapping and configurable hotbar composition.
- Left-handed / mirrored layouts for persistent action clusters.
- Adjustable control opacity if controls cover gameplay.
- Alternative targeting methods for players who struggle with precise taps.
- Reduced reliance on long-press or multi-finger gestures.
- Controller navigation that exposes the same game semantics as touch rather than a separate second-class UI.
## 13. Mobile interruption is a design constraint
- Autosave or checkpoint often enough that a phone call is not catastrophic.
- Pause or safely suspend when the app backgrounds when game rules permit.
- Restore UI context—not just world state—after interruption.
- Keep reconnect flows fast and understandable for online games.
- Design common activities so a useful session can fit into a few minutes even if the original PC game expects long play sessions.
## 14. Genre-specific adaptations
### A. Isometric / direct-manipulation RPGs and MMOs
- Tap or virtual-stick movement depending on combat pacing and world density.
- Entity snapping and explicit current target.
- Contextual interact button.
- Configurable combat/utility hotbar.
- Inventory and paperdoll as large temporary panels rather than tiny floating windows.
- Minimap in a stable top corner; larger map as an overlay.
- Long-press or explicit context button for desktop right-click behaviors.
### B. Keyboard-heavy CRPGs / strategy RPGs
- Promote the command vocabulary into visible action groups.
- Use pause/slow-time generously when precision and menu navigation coincide.
- Create contextual action menus rather than reproducing dozens of hotkeys.
- Use tabs and sheets for party, inventory, spells, character data, and logs.
- Offer target cycling and selection aids in dense encounters.
### C. RTS / management / simulation
- Prioritize camera/navigation gestures and reliable object selection.
- Use tap-to-select followed by contextual command trays.
- Create an explicit multi-select mode rather than requiring modifier-key emulation.
- Use zoom-dependent detail: show more controls and labels as the player zooms in.
- Favor inspectors and bottom sheets over miniature desktop sidebars.
### D. Action games / shooters
- Keep the permanent action set small and reachable.
- Use aim assist, target snapping, auto-fire, gyro, or contextual actions as appropriate to the game.
- Allow extensive control placement customization.
- Ensure fingers do not obscure the exact region the player must inspect or aim at.
- Design controller support early, but validate that touch remains a complete experience.
## 15. Anti-patterns
The shrink-ray port. Desktop HUD scaled down until technically everything fits.
The on-screen keyboard. A grid of tiny buttons representing dozens of PC hotkeys.
The precision tax. Requiring the player to hit tiny moving sprites or narrow inventory slots.
The mystery gesture. Critical actions hidden behind unlabeled swipes, multi-finger taps, or holds.
The permanent modal maze. Common gameplay requires repeatedly entering full-screen nested menus.
The finger blind spot. Controls or finger placement cover the exact part of the world the player needs to see.
The mode trap. The same tap silently does different things because an obscure persistent mode is active.
Controller as an excuse. Touch works poorly because the design assumes serious players will attach a controller.
Fake fidelity. Desktop window borders, title bars, and tiny pixel grids are preserved even when they damage usability.
No interruption model. Backgrounding, rotation, reconnect, or keyboard appearance destroys state or creates accidental input.
## 16. Worked example: Ultima Online-style mobile client
Ultima Online is an unusually useful stress test because the classic client assumes mouse precision, right-click behavior, draggable gumps, extensive hotkeys/macros, targeting cursors, and many simultaneously visible information windows.
### Recommended mobile composition
- World viewport remains visually dominant.
- Minimap anchors to a top corner with tap-to-expand behavior.
- Player health/resources are glanceable but compact.
- Lower-right configurable action cluster: Target Closest, Attack Last Target, Bandage Self, Use/Interact, plus 1–3 character-specific actions.
- Optional lower-left movement control when tap-to-move is insufficient for combat; otherwise keep the world directly tappable.
- Current target receives a strong world highlight plus compact HUD identity/health.
- Inventory, paperdoll, spellbook, skills, journal, and macros become larger temporary panels or tabbed hubs rather than overlapping desktop gumps.
- Common container actions use tap-select and contextual actions; drag-and-drop remains available as an optional shortcut.
- Right-click semantics translate to long-press or a visible Context action depending on frequency and ambiguity.
- Target cursor flows get explicit Cancel, Last Target, Closest Target, and cycle-target affordances.
### A melee-character default action set
- Primary: Attack / Attack Last Target
- Target Closest Hostile
- Bandage Self
- Use / Interact
- War / Peace toggle if still mechanically necessary
- Character-specific special or potion slot
- Expandable hotbar for less frequent macros
This is a good example of preserving the underlying UO command system while refusing to make the player operate it like a 1997 Windows client.
## 17. Requirements language for coding agents
When handing a port task to Codex or another coding agent, use requirements framed around interaction semantics. Useful language:
- “Do not literally scale desktop UI; redesign controls for touch while preserving game mechanics.”
- “Every critical action must have a touch path that does not rely on hover, right-click, keyboard modifiers, or pixel-precise targeting.”
- “Identify Tier A/B/C/D actions before implementing HUD placement.”
- “Use large semantic hit regions around world entities.”
- “Support configurable control placement/scale for the persistent action layer.”
- “Treat portrait and landscape as separate layout compositions.”
- “Do not make gestures the sole discoverability path for critical actions.”
- “Validate background/resume, software-keyboard appearance, and safe-area changes.”

## 18. Port review checklist
Use this as a design review before declaring a screen or interaction “mobile-ready.”
### Player intent
☐ Have we listed the player intents on this screen?
☐ Are frequent + urgent intents permanently accessible?
☐ Are rare intents progressively disclosed?
### Touch geometry
☐ Are primary controls comfortably thumb-reachable?
☐ Are interactive targets finger-sized rather than art-sized?
☐ Can the player operate important controls without covering the action?
### Input translation
☐ Does every hover state have a touch equivalent?
☐ Does every right-click action have a touch equivalent?
☐ Have keyboard shortcuts been translated into a mobile command model rather than an on-screen keyboard?
☐ Can precision-heavy desktop actions be expressed through intent/snap/context?
### World interaction
☐ Does a world tap have a predictable meaning?
☐ Is the current selection/target obvious?
☐ Can the player cancel targeting or modal states easily?
### Active play
☐ Can combat-critical actions be performed without opening a deep menu?
☐ Can the player target reliably in crowded scenes?
☐ Are cooldowns, disabled states, and feedback immediately legible?
### HUD and panels
☐ Is every permanent HUD element continuously useful?
☐ Could any permanent panel become a temporary sheet/drawer/overlay?
☐ Does opening a panel preserve world context when useful?
### Inventory
☐ Are item cells/hit regions large enough?
☐ Is drag-and-drop optional rather than mandatory for routine actions?
☐ Are common actions exposed contextually?
### Orientation
☐ Has each supported orientation been composed intentionally?
☐ Does rotation preserve state and avoid accidental input?
### Accessibility/customization
☐ Can controls be scaled or remapped where useful?
☐ Is left-handed play considered?
☐ Are important states understandable without color alone?
☐ Are critical actions possible without obscure multi-finger gestures?
### Mobile lifecycle
☐ Does background/resume preserve game and UI state?
☐ Does the software keyboard avoid hiding required controls/content?
☐ Are reconnect and interruption flows safe?
### Controller
☐ Is touch a complete first-class experience?
☐ Does controller input map to the same semantic actions and UI states?
## 19. Decision heuristic
When uncertain between two mobile interactions, choose the option that minimizes these costs in order:
1. Accidental destructive action.
1. Failure to express the intended target/action.
1. Extra steps during time-sensitive play.
1. Finger occlusion of important world information.
1. Hidden or undiscoverable behavior.
1. Visual departure from the original desktop UI.
That final ordering is intentional: visual fidelity is valuable, but interaction fidelity and player intent matter more.
## 20. How to evolve this playbook
Treat this document as a living pattern library. After each port or prototype, add three things: (1) patterns that worked, (2) patterns that failed and why, and (3) game-specific exceptions that should become reusable rules. Over time, the playbook should become less theoretical and more like a catalog of proven interaction components.

| Element | Default expectation | Why |
| --- | --- | --- |
| Primary touch target | Aim for ~44×44 pt or larger | Supports reliable thumb input and aligns with platform conventions. |
| Time-sensitive actions | Reachable without opening a full menu | Latency in UI navigation becomes gameplay difficulty. |
| Text | Scale for phone viewing distance, not desktop density | Literal desktop scaling produces unreadable, fatiguing UI. |
| Critical state | Visible without hover or memory | The player should not need to remember hidden status while acting. |
| Gesture-only commands | Avoid for critical actions | Gestures can be fast, but are hard to discover and easy to trigger accidentally. |


| Desktop convention | Preferred iOS translation | Notes |
| --- | --- | --- |
| Hover | Tap, tap-and-hold, persistent label, info button, or contextual preview | Never make essential information hover-only. |
| Right-click | Tap-and-hold or explicit context/action button | Use hold only when accidental activation is low-risk. |
| Double-click | Single contextual tap, explicit action, or optional double-tap | Avoid requiring double-tap for critical timing. |
| Mouse wheel | Pinch, drag, on-screen stepper, edge control, or automatic scaling | Choose based on whether the intent is zoom, scroll, cycle, or change quantity. |
| Keyboard hotkey | Hotbar slot, radial menu, command palette, contextual action, or controller binding | Expose the most important hotkeys directly; do not recreate the keyboard. |
| Modifier + click | Mode button, selection tray, multi-select mode, or contextual popover | Persistent modes need conspicuous state feedback. |
| Drag-and-drop | Tap-select → tap-destination; drag with large targets; auto-place; action menu | Drag can remain optional for satisfying direct manipulation. |
| Pixel-precise targeting | Snap targeting, entity hit regions, target nearest, cycle target, aim assist | The player should express who/what they mean, not prove touchscreen dexterity. |
| Window management | Sheets, drawers, tabbed panels, temporary overlays, split views | Multiple floating windows rarely survive phone-size screens intact. |
| Tooltip-heavy UI | Inline labels, expandable details, info panels | Reveal detail progressively rather than hiding basic meaning. |


|  | Portrait | Landscape |
| --- | --- | --- |
| Strengths | One-handed potential; excellent vertical lists; natural lower-corner thumb zones; discrete top/bottom information regions. | Wide world view; room for side panels; two-thumb play; easier migration for games authored around 16:9. |
| Risks | Narrow combat view; side panels are expensive; controls can crowd the lower half. | Long reach to center/top; fingers can cover the scene; temptation to preserve too much PC chrome. |
| Good fits | Turn-based RPGs, roguelikes, card games, tactical/menu-heavy play, some isometric games. | Action RPGs, MMOs, shooters, RTS-like games, games with strong horizontal spatial awareness. |
