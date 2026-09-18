# Ultimatum Experience Profiles

Status: Draft product and technical specification  
Applies initially to: *Ultima IV Mobile*  
Intended reuse: Future games in the Ultimatum series

## 1. Purpose

Experience Profiles let players choose how much convenience and guidance the
game provides without creating separate editions of the game. The three
profiles are **Classic**, **Ultimatum**, and **Assisted**.

All profiles run the same game executable, engine, content, and save system.
Profiles select defaults for independently implemented features. Players may
change eligible settings individually after choosing a profile.

The central product rule is:

> Reduce bookkeeping, repetition, and touch friction, but never solve
> Britannia's mysteries on the player's behalf unless the player explicitly
> enables an assistance feature.

This specification governs the profiles and their settings. Candidate features
are catalogued separately in [QUALITY_OF_LIFE_IDEAS.md](QUALITY_OF_LIFE_IDEAS.md).
Touch and layout decisions remain governed by
[PC_to_iOS_Interface_Playbook.md](PC_to_iOS_Interface_Playbook.md).

## 2. Terminology

- **Experience Profile**: A named collection of defaults: Classic, Ultimatum,
  or Assisted.
- **Profile Default**: The value a profile supplies for an eligible setting.
- **Player Override**: A player-selected value that differs from the active
  profile's default.
- **Customized Profile**: An active profile with one or more player overrides.
  The player-facing label is, for example, `Ultimatum — Customized`.
- **Core Mobile Standard**: Required behavior shared by all profiles. It is not
  removable through experience settings.
- **Assistance Feature**: An optional feature that supplies guidance or
  information beyond the classic experience.
- **Presentation Feature**: A feature that changes how earned information or
  feedback is displayed without changing game rules.
- **Gameplay Rule**: A mechanic that affects simulation, probabilities,
  resources, turn accounting, progression, or consequences. Gameplay rules are
  not Experience Profile settings unless a future specification explicitly
  promotes them to that role.

Do not call profiles "versions" in product text, code, documentation, or team
discussion. Version refers to an application release, data schema, or save
format revision.

## 3. Product goals

1. Make Ultimatum the confident, recommended mobile-first experience.
2. Let players preserve classic information limits and challenge without
   accepting an intentionally awkward touch interface.
3. Offer additional help without quietly changing the game's underlying rules.
4. Allow safe profile switching without duplicating, corrupting, or discarding
   an adventure.
5. Keep individual settings understandable and avoid an unbounded matrix of
   subtly different game behaviors.
6. Establish vocabulary and architecture reusable across the Ultimatum series.

## 4. Non-goals

- Maintaining three application builds or three engine forks.
- Reproducing keyboard or desktop friction as part of Classic.
- Allowing accessibility, data safety, or minimum touch usability to be
  disabled for authenticity.
- Treating every quality-of-life idea as independently configurable.
- Using profiles as difficulty levels.
- Promising that every profile setting exists in every Ultimatum game.
- Sharing implementation details that couple otherwise unrelated game engines.

## 5. The three profiles

### 5.1 Classic

Classic preserves the original game's information boundaries and player
bookkeeping where those are meaningful parts of discovery. It uses the same
touch-native controls, readable panels, accessibility support, and safe mobile
lifecycle behavior as every other profile.

Classic should disable optional features that reveal, organize, predict, or
automate more than the original experience. It must not deliberately restore
tiny controls, keyboard dependence, unsafe saves, illegible text, or fragile
interaction patterns.

### 5.2 Ultimatum

Ultimatum is the default and recommended profile. It is the authored expression
of the series: faithful game rules and discovery delivered through a polished,
mobile-first interface.

Ultimatum should reduce repetitive input and external note-taking, organize
information the player has legitimately encountered, and present immediate
game state clearly. It should not reveal undiscovered answers, locations,
topics, recipes, or hidden virtue values.

### 5.3 Assisted

Assisted includes the Ultimatum defaults and enables additional optional
guidance. Assistance may summarize discovered objectives, expose exact
navigation information, protect against consequential mistakes, or respond to
repeated failure.

Assisted features must be described honestly. They may reduce deduction or
bookkeeping, but should not alter combat probabilities, prices, resource costs,
virtue calculations, or other simulation rules merely because Assisted is
selected.

## 6. Core Mobile Standard

The following categories are foundational product behavior, not profile
options:

- Finger-sized hit regions and reliable touch targeting.
- Touch-native navigation and panels for required game actions.
- Legible text, safe-area handling, and intentional portrait/landscape layouts.
- A persistent, glanceable party roster during world exploration and combat,
  showing every member's current HP and condition.
- A visible way to perform every critical action; gestures may be shortcuts but
  not the sole path.
- VoiceOver semantics and color-independent critical state where supported.
- Safe pause, backgrounding, checkpoint publication, recovery, and resume.
- Validation that prevents corrupt or incompatible data from being loaded.
- Confirmation or recovery for destructive application-level actions such as
  replacing an adventure.

A Classic player receives all of these. If a proposed switch would disable one,
the proposal requires a design review and normally should be rejected.

## 7. Eligibility rules for configurable features

A feature may become a profile setting when all of the following are true:

1. Reasonable players could prefer either value.
2. Disabling it does not violate the Core Mobile Standard.
3. Its effect can be described in a short, concrete settings label and help
   sentence.
4. The implementation can define when changes take effect.
5. The enabled and disabled states can both be tested and maintained.
6. Turning it off does not require deleting player discoveries or progress.

Prefer a confident product default when these conditions are not met. Avoid
settings that expose internal implementation details, overlap another setting,
or differ so subtly that players cannot predict the result.

## 8. Feature classification

Every proposed feature must be assigned exactly one primary classification
before implementation:

| Classification | Example | Profile behavior |
|---|---|---|
| Core mobile | Large touch targets | Always enabled |
| Accessibility | Text scale, reduced motion | Independently configurable; never made less available by Classic |
| Presentation | Filter routine journal messages | May vary by profile |
| Convenience | Batch reagent mixing | May vary by profile if game rules and costs are preserved |
| Assistance | Map coordinates, objective reminders | Normally off in Classic and opt-in or on in Assisted |
| Gameplay rule | Combat probability or resource-cost change | Outside this system |

When a feature spans categories, use the classification with the greatest
effect on player knowledge or game outcomes. For example, a map is presentation;
automatically marking an undiscovered dungeon is assistance.

## 9. Initial profile policy

The following matrix defines direction, not a commitment that every listed
feature ships in the first release:

| Capability | Classic | Ultimatum | Assisted |
|---|---:|---:|---:|
| Core Mobile Standard | On | On | On |
| Persistent party HP and condition roster | On | On | On |
| Three independent adventure save slots | On | On | On |
| Expandable exploration-based map | Off | On | On |
| Player-created map pins | Off | On | On |
| Exact map coordinates | Off | Off | On |
| Discovery-gated journal | Off or minimal | On | On |
| Filter routine movement messages | Off | On | On |
| Journal search and cross-reference of encountered facts | Off | On | On |
| Contextual touch actions | On | On | On |
| Combat target and range previews | Off | On | On |
| Configurable companion battle tactics | Off | Available; Manual default | Available; suggested tactics |
| Castability explanations | Off | On | On |
| Objective reminders derived from encountered facts | Off | Off | On |
| Failure-sensitive hints | Off | Off | On |
| Accessibility and control customization | Available | Available | Available |
| Safe checkpoints and recovery | On | On | On |

The exact Classic treatment of the journal requires a feature-level design
decision. Existing recorded journal data must remain intact even when its
enhanced presentation is unavailable.

### 9.1 Persistent party status HUD

The DOS interface kept party health and condition visible during play. The
mobile interface must preserve that information boundary rather than treating
the roster as optional convenience or assistance. It has no profile-managed
setting and does not contribute to a Customized profile.

The recommended mobile component is a compact grid of party-member cells:

- Show all current party members, in party order, whenever the world or combat
  view is the active gameplay surface. A full-screen panel or software keyboard
  may temporarily replace it.
- Each cell shows the member's name or an unambiguous abbreviation, current HP,
  and engine condition. Healthy, Poisoned, Asleep, and Dead must be
  distinguishable without relying on color alone.
- Use a thin proportional HP bar as redundant glanceable feedback, while
  retaining the numeric HP value as the authoritative reading. Low-health
  styling must not imply a new rule or hidden threshold.
- Mark the active combatant with both shape and text or icon treatment. Do not
  communicate the active member only through hue.
- In portrait, prefer four columns by two rows between the world viewport and
  the lower controls. In landscape, prefer two columns by four rows in the
  upper-left status rail, above the movement controls. Layouts with fewer than
  eight members leave unused cells absent rather than displaying placeholders.
- Make each complete cell a finger-sized target that opens that member's party
  details. This is a touch translation of party inspection, not a separate
  profile feature.
- Keep location, moon, wind, food, gold, and ship status outside the member
  cells so resource summaries can adapt independently without obscuring party
  health.

The first implementation should favor readable text and stable geometry over
portraits or decorative art. Character portraits may be evaluated later only
if they do not reduce map size, status legibility, or touch target size.

### 9.2 Multiple adventure save slots

Three independent adventure slots are part of the Core Mobile Standard. They
are not a convenience or assistance toggle and remain available under every
Experience Profile. Slot 1 retains the original mobile save root so an existing
adventure appears without migration. Each additional slot owns a separate
immutable checkpoint tree, including its CURRENT and PREVIOUS pointers,
journal, explored map, and later adventure-owned feature data.

Journey Onward identifies occupied slots by number, Avatar name, and party
size. New Game shows occupied and empty slots and requires explicit
confirmation before publishing a new adventure over an occupied slot. Manual
and periodic saves always name the active slot in their feedback. Selecting or
cancelling a slot never advances a turn.

### 9.3 Bump interactions

Bump interaction is part of the Contextual touch actions row and therefore has
the same default in Classic, Ultimatum, and Assisted. It translates a blocked
movement attempt into an unambiguous adjacent Talk or Open intent without
changing simulation rules or revealing information. It remains an independent
Controls preference and does not customize the active profile. The complete
target, safety, turn-accounting, and verification requirements are defined in
[BUMP_INTERACTION_SPEC.md](BUMP_INTERACTION_SPEC.md).

## 10. Player experience

### 10.1 First selection

- A new installation defaults to **Ultimatum**.
- The profile chooser briefly explains all three choices and marks Ultimatum as
  recommended.
- Choosing a profile does not require understanding individual settings.
- Accessibility preferences may be chosen independently and must not silently
  change the profile label.

### 10.2 Settings organization

The settings root shows the active Experience Profile followed by grouped
settings such as Controls, Interface, Journal, Exploration, Combat, Assistance,
Accessibility, and Audio.

Each profile-managed setting indicates its current value. When the player
changes one away from its profile default, the profile label becomes
`<Profile> — Customized`. The interface should offer **Restore Profile
Defaults** without resetting unrelated accessibility, audio, or control-layout
preferences.

### 10.3 Switching profiles

Selecting a new profile presents a concise summary of materially changed
features. Confirmation applies the new profile's defaults as one transaction.
The operation must never create a second adventure or rewrite historical save
generations.

The initial implementation should reset profile-managed overrides when a new
profile is chosen. This makes the result predictable: choosing Classic means
the defined Classic experience rather than Classic plus invisible leftovers
from Assisted. A later design may preserve a separate override set per profile,
but only if the additional complexity proves valuable.

### 10.4 When changes take effect

Each setting declares one of these policies:

- **Immediate**: Safe visual or feedback changes apply while the settings panel
  is open.
- **After dismissal**: Layout or input changes apply after leaving Settings so
  active touch state can be cancelled cleanly.
- **At safe boundary**: Changes affecting an interaction flow apply after the
  current conversation, targeting operation, combat turn, or equivalent modal
  state ends.
- **After reload**: Used only when live reconfiguration would be unsafe or
  prohibitively complex. The UI must say so before confirmation.
- **New adventure only**: Reserved for actual gameplay rules outside normal
  Experience Profile scope.

Profile switching uses the strictest policy required by any changed setting.
The player must never receive a half-applied profile.

If switching profiles disables companion automation during combat, the change
takes effect at the next character-turn boundary. No queued automated action
may execute after that boundary. Saved tactic assignments remain adventure data
and are not erased.

## 11. Configuration model

Profiles must be data-driven definitions resolved through a central settings
service. Code should query semantic capabilities rather than profile names.

Conceptually:

```text
effective value = profile default + eligible player override
```

Required configuration concepts:

```cpp
enum class ExperienceProfile { Classic, Ultimatum, Assisted };
enum class ApplyPolicy { Immediate, AfterDismissal, SafeBoundary, Reload };

struct FeatureDefinition {
    FeatureKey key;
    FeatureClass classification;
    ApplyPolicy applyPolicy;
    bool classicDefault;
    bool ultimatumDefault;
    bool assistedDefault;
};
```

The concrete representation may differ, but it must provide:

- Stable string keys for persisted settings.
- A schema version independent of the application version.
- Defaults centralized in one auditable location.
- Validation and safe fallback for unknown or malformed values.
- Explicit migrations when a setting is renamed or changes meaning.
- The ability to determine whether the active profile is customized.
- A single effective-value API consumed by the C/C++ engine and Objective-C++
  interface layer.

Do not implement features with scattered checks such as
`profile == Assisted`. Ask for the actual capability, such as
`showMapCoordinates()`. This keeps profiles composable and makes overrides
reliable.

## 12. Persistence ownership

Settings and adventure data have different lifetimes and must not be conflated.

### 12.1 Device-level preferences

Store these outside adventure snapshots:

- Selected Experience Profile.
- Player overrides.
- Control placement, scale, handedness, and opacity.
- Text scale, reduced motion, haptic preference, and other accessibility or
  comfort settings.
- Audio preferences.

These normally apply to every adventure on the device.

The current engine uses a flat `settings` file. The profile system may extend
that format or introduce a separate versioned mobile-preferences file, but it
must retain backward-compatible defaults and portable test coverage. Native
`NSUserDefaults` must not become the only source of values needed by portable
engine code.

### 12.2 Adventure-owned data

Store facts learned or created during play with the adventure:

- Journal discoveries and recorded passages.
- Explored map cells and discovered locations.
- Player map pins and personal notes.
- Known recipes or other legitimately learned reference data.
- Hint history when needed to avoid repetition.
- Per-character companion battle tactics and explicit automation policies.

Disabling a feature hides or stops using this data; it does not erase it. A
player may switch from Ultimatum to Classic and later return without losing the
journal, explored map, or pins previously accumulated.

Adventure-owned additions must participate in the existing immutable checkpoint
generation. They must be included in the generation's allowed-file list,
publication validation, recovery validation, and retention tests. A small
versioned manifest should identify optional feature-data schemas and permit
older saves to load with empty defaults.

## 13. Save compatibility and authenticity

- The underlying *Ultima IV* simulation save remains profile-neutral wherever
  practical.
- Profiles must not fork an adventure into incompatible Classic, Ultimatum, and
  Assisted save formats.
- Loading a save under a different profile is supported.
- Profile selection is not copied into every immutable checkpoint unless a
  future gameplay-rule feature makes per-adventure provenance necessary.
- Unknown optional feature data must be preserved when feasible and must never
  cause the core adventure to be silently discarded.
- Downgrade behavior must be defined before a new feature-data schema ships.

If the product later introduces settings that change actual game rules, those
settings require a separate ruleset specification. They should be recorded in
adventure metadata, applied only at defined boundaries, and must not be
presented as ordinary interface customization.

## 14. Runtime switching requirements

Applying a profile or override must follow a transactional sequence:

1. Resolve and validate the complete proposed configuration.
2. Compare it with the active effective configuration.
3. Determine the strictest required application boundary.
4. Cancel or complete incompatible transient UI state when appropriate.
5. Persist preferences atomically.
6. Apply the entire configuration.
7. Refresh affected UI and announce material accessibility changes.
8. If application fails, retain or restore the previous complete configuration
   and report that the change did not take effect.

At minimum, switching must be tested while on the world map, in combat, during
a conversation, with a native panel open, after device rotation, and after
background/resume. Unsafe contexts may defer application; they may not partially
switch behavior.

## 15. Engineering integration for Ultima IV

The current code already has useful foundations:

- `src/settings.h` and `src/settings.c` contain a portable settings model and
  persisted defaults.
- `ios/zu4_ios_ui.mm` is the primary native HUD and semantic-action boundary.
- `ios/topic_panel.mm` owns native conversations and journal presentation.
- `src/save_snapshot.h` publishes immutable save generations through `CURRENT`
  and `PREVIOUS` pointers.
- Save validation and snapshot regression tests already exercise recovery
  behavior.

Implementation should evolve these boundaries rather than create parallel game
paths. A recommended sequence is:

1. Add the versioned profile/override model with no gameplay behavior changes.
2. Add a native profile chooser and grouped settings UI.
3. Convert one low-risk existing preference, such as movement-message filtering,
   to the central effective-value API.
4. Add migration and profile-switching tests.
5. Convert or add QoL capabilities individually, assigning classification,
   defaults, ownership, and apply policy to each.
6. Add adventure feature-data files only when their corresponding feature is
   implemented, extending checkpoint validation at the same time.

## 16. Test requirements

Every profile-managed feature requires tests for:

- Its default under Classic, Ultimatum, and Assisted.
- An explicit override in both directions.
- Customized-profile detection and restoration of defaults.
- Persistence across relaunch.
- Behavior when its persisted key is missing, malformed, or from an older
  schema.
- Switching into and out of the feature while adventure data already exists.
- Its declared application boundary.

The system as a whole requires:

- One automated baseline configuration test per profile.
- Migration tests from the current settings file.
- Atomic-write failure tests.
- Snapshot publication and recovery tests for each adventure-owned feature file.
- UI checks in portrait and landscape, including larger text sizes.
- VoiceOver labels and values for the profile chooser and every setting.
- A manual playthrough checklist verifying that Classic remains touch-usable,
  Ultimatum does not reveal undiscovered information, and Assisted descriptions
  match actual behavior.

Pairwise testing may be used for combinations of unrelated overrides. Features
that interact directly require explicit combination tests.

## 17. Acceptance criteria for the first profile release

The initial Experience Profiles release is complete when:

1. Classic, Ultimatum, and Assisted are selectable and accurately described.
2. Ultimatum is the default for a new installation.
3. At least one real setting differs among the profiles.
4. Players can override eligible settings and see the Customized label.
5. Restore Profile Defaults affects only profile-managed settings.
6. Switching profiles neither creates a new adventure nor loses adventure-owned
   information.
7. Existing installations migrate without losing their prior settings or save.
8. The effective settings API is shared by the engine and native UI.
9. Profile selection and overrides survive relaunch.
10. Automated tests cover defaults, overrides, migration, persistence, and safe
    switching.

## 18. Decision record

- The public and internal term is **Experience Profile**.
- The profiles are **Classic**, **Ultimatum**, and **Assisted**.
- **Ultimatum** is the recommended default.
- Profiles are configuration baselines, not builds, branches, save types,
  difficulty levels, or separate products.
- Accessibility and the Core Mobile Standard remain available in every profile.
- Player knowledge is preserved when its presentation feature is disabled.
- Actual gameplay-rule variants require a separate system and specification.
