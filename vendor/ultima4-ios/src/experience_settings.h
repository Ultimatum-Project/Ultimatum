#ifndef EXPERIENCE_SETTINGS_H
#define EXPERIENCE_SETTINGS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct SettingsData;

#define ZU4_EXPERIENCE_SCHEMA_VERSION 3

typedef enum Zu4ExperienceProfile {
    ZU4_EXPERIENCE_CLASSIC = 0,
    ZU4_EXPERIENCE_ULTIMATUM = 1,
    ZU4_EXPERIENCE_ASSISTED = 2
} Zu4ExperienceProfile;

typedef enum Zu4GraphicsTheme {
    ZU4_GRAPHICS_EGA = 0,
    ZU4_GRAPHICS_VGA = 1
} Zu4GraphicsTheme;

typedef enum Zu4ExperienceFeatureKey {
    ZU4_FEATURE_FILTER_MOVEMENT_MESSAGES = 0,
    ZU4_FEATURE_GRAPHICS_THEME = 1,
    ZU4_FEATURE_EXPLORATION_MAP = 2,
    ZU4_FEATURE_MAP_PINS = 3
} Zu4ExperienceFeatureKey;

typedef enum Zu4ExperienceFeatureClass {
    ZU4_FEATURE_PRESENTATION = 0,
    ZU4_FEATURE_CONVENIENCE = 1
} Zu4ExperienceFeatureClass;

typedef enum Zu4ExperienceApplyPolicy {
    ZU4_APPLY_IMMEDIATE = 0,
    ZU4_APPLY_AFTER_DISMISSAL,
    ZU4_APPLY_SAFE_BOUNDARY,
    ZU4_APPLY_RELOAD
} Zu4ExperienceApplyPolicy;

typedef struct Zu4ExperienceFeatureDefinition {
    Zu4ExperienceFeatureKey key;
    const char *stableKey;
    Zu4ExperienceFeatureClass classification;
    Zu4ExperienceApplyPolicy applyPolicy;
    int profileDefaults[3];
} Zu4ExperienceFeatureDefinition;

typedef struct Zu4ExperiencePreferences {
    int schemaVersion;
    Zu4ExperienceProfile profile;
    /* -1 inherits from the active profile; 0 and 1 are explicit overrides. */
    int filterMovementMessagesOverride;
    int graphicsThemeOverride;
    int explorationMapOverride;
    int mapPinsOverride;
} Zu4ExperiencePreferences;

extern Zu4ExperiencePreferences experiencePreferences;

int zu4_experience_feature_count(void);
const Zu4ExperienceFeatureDefinition *zu4_experience_feature_definition(int index);

void zu4_experience_reset_new_install(void);
void zu4_experience_migrate_legacy(bool hadSettingsFile,
                                   int legacyFilterMovementMessages,
                                   int legacyGraphicsTheme);
bool zu4_experience_validate(void);
void zu4_experience_apply(struct SettingsData *target);

Zu4ExperienceProfile zu4_experience_profile(void);
const char *zu4_experience_profile_key(Zu4ExperienceProfile profile);
const char *zu4_experience_profile_name(Zu4ExperienceProfile profile);
bool zu4_experience_profile_from_key(const char *key, Zu4ExperienceProfile *profile);
bool zu4_experience_set_profile(Zu4ExperienceProfile profile, struct SettingsData *target);
bool zu4_experience_is_customized(void);
void zu4_experience_restore_defaults(struct SettingsData *target);

bool zu4_experience_filter_movement_messages(void);
Zu4GraphicsTheme zu4_experience_graphics_theme(void);
bool zu4_experience_exploration_map(void);
bool zu4_experience_map_pins(void);
bool zu4_experience_set_filter_movement_messages(bool enabled, struct SettingsData *target);
bool zu4_experience_set_graphics_theme(Zu4GraphicsTheme theme, struct SettingsData *target);
bool zu4_experience_set_exploration_map(bool enabled, struct SettingsData *target);
bool zu4_experience_set_map_pins(bool enabled, struct SettingsData *target);

#ifdef __cplusplus
}
#endif

#endif
