#include "experience_settings.h"

#include <string.h>

#include "settings.h"

Zu4ExperiencePreferences experiencePreferences;

static const Zu4ExperienceFeatureDefinition featureDefinitions[] = {
    { ZU4_FEATURE_FILTER_MOVEMENT_MESSAGES, "presentation.filterMovementMessages",
      ZU4_FEATURE_PRESENTATION, ZU4_APPLY_IMMEDIATE, { 0, 1, 1 } },
    { ZU4_FEATURE_GRAPHICS_THEME, "graphics.theme",
      ZU4_FEATURE_PRESENTATION, ZU4_APPLY_IMMEDIATE,
      { ZU4_GRAPHICS_EGA, ZU4_GRAPHICS_VGA, ZU4_GRAPHICS_VGA } },
    { ZU4_FEATURE_EXPLORATION_MAP, "navigation.explorationMap",
      ZU4_FEATURE_PRESENTATION, ZU4_APPLY_IMMEDIATE, { 0, 1, 1 } },
    { ZU4_FEATURE_MAP_PINS, "navigation.playerMapPins",
      ZU4_FEATURE_CONVENIENCE, ZU4_APPLY_IMMEDIATE, { 0, 1, 1 } }
};

int zu4_experience_feature_count(void) {
    return (int)(sizeof(featureDefinitions) / sizeof(featureDefinitions[0]));
}

const Zu4ExperienceFeatureDefinition *zu4_experience_feature_definition(int index) {
    if (index < 0 || index >= zu4_experience_feature_count())
        return NULL;
    return &featureDefinitions[index];
}

static int profile_default(Zu4ExperienceFeatureKey key, Zu4ExperienceProfile profile) {
    int i;
    for (i = 0; i < zu4_experience_feature_count(); ++i) {
        if (featureDefinitions[i].key == key)
            return featureDefinitions[i].profileDefaults[(int)profile];
    }
    return 0;
}

static bool profile_filter_default(Zu4ExperienceProfile profile) {
    return profile_default(ZU4_FEATURE_FILTER_MOVEMENT_MESSAGES, profile) != 0;
}

static Zu4GraphicsTheme profile_graphics_default(Zu4ExperienceProfile profile) {
    return (Zu4GraphicsTheme)profile_default(ZU4_FEATURE_GRAPHICS_THEME, profile);
}

void zu4_experience_reset_new_install(void) {
    experiencePreferences.schemaVersion = ZU4_EXPERIENCE_SCHEMA_VERSION;
    experiencePreferences.profile = ZU4_EXPERIENCE_ULTIMATUM;
    experiencePreferences.filterMovementMessagesOverride = -1;
    experiencePreferences.graphicsThemeOverride = -1;
    experiencePreferences.explorationMapOverride = -1;
    experiencePreferences.mapPinsOverride = -1;
}

void zu4_experience_migrate_legacy(bool hadSettingsFile,
                                   int legacyFilterMovementMessages,
                                   int legacyGraphicsTheme) {
    if (!hadSettingsFile)
        return;

    /* Existing installs become Ultimatum while retaining every old preference. */
    bool filter = legacyFilterMovementMessages != 0;
    Zu4GraphicsTheme graphics = legacyGraphicsTheme == 1 ? ZU4_GRAPHICS_VGA : ZU4_GRAPHICS_EGA;
    if (filter != profile_filter_default(experiencePreferences.profile))
        experiencePreferences.filterMovementMessagesOverride = filter ? 1 : 0;
    if (graphics != profile_graphics_default(experiencePreferences.profile))
        experiencePreferences.graphicsThemeOverride = (int)graphics;
}

bool zu4_experience_validate(void) {
    bool valid = true;
    if (experiencePreferences.schemaVersion < 1 ||
        experiencePreferences.schemaVersion > ZU4_EXPERIENCE_SCHEMA_VERSION) {
        experiencePreferences.schemaVersion = ZU4_EXPERIENCE_SCHEMA_VERSION;
        valid = false;
    }
    if (experiencePreferences.profile < ZU4_EXPERIENCE_CLASSIC ||
        experiencePreferences.profile > ZU4_EXPERIENCE_ASSISTED) {
        experiencePreferences.profile = ZU4_EXPERIENCE_ULTIMATUM;
        valid = false;
    }
    if (experiencePreferences.filterMovementMessagesOverride < -1 ||
        experiencePreferences.filterMovementMessagesOverride > 1) {
        experiencePreferences.filterMovementMessagesOverride = -1;
        valid = false;
    }
    if (experiencePreferences.graphicsThemeOverride < -1 ||
        experiencePreferences.graphicsThemeOverride > 1) {
        experiencePreferences.graphicsThemeOverride = -1;
        valid = false;
    }
    if (experiencePreferences.explorationMapOverride < -1 ||
        experiencePreferences.explorationMapOverride > 1) {
        experiencePreferences.explorationMapOverride = -1;
        valid = false;
    }
    if (experiencePreferences.mapPinsOverride < -1 ||
        experiencePreferences.mapPinsOverride > 1) {
        experiencePreferences.mapPinsOverride = -1;
        valid = false;
    }
    return valid;
}

bool zu4_experience_filter_movement_messages(void) {
    if (experiencePreferences.filterMovementMessagesOverride >= 0)
        return experiencePreferences.filterMovementMessagesOverride != 0;
    return profile_filter_default(experiencePreferences.profile);
}

Zu4GraphicsTheme zu4_experience_graphics_theme(void) {
    if (experiencePreferences.graphicsThemeOverride >= 0)
        return (Zu4GraphicsTheme)experiencePreferences.graphicsThemeOverride;
    return profile_graphics_default(experiencePreferences.profile);
}

bool zu4_experience_exploration_map(void) {
    if (experiencePreferences.explorationMapOverride >= 0)
        return experiencePreferences.explorationMapOverride != 0;
    return profile_default(ZU4_FEATURE_EXPLORATION_MAP, experiencePreferences.profile) != 0;
}

bool zu4_experience_map_pins(void) {
    if (experiencePreferences.mapPinsOverride >= 0)
        return experiencePreferences.mapPinsOverride != 0;
    return profile_default(ZU4_FEATURE_MAP_PINS, experiencePreferences.profile) != 0;
}

void zu4_experience_apply(SettingsData *target) {
    if (!target)
        return;
    zu4_experience_validate();
    target->filterMoveMessages = zu4_experience_filter_movement_messages();
    target->videoType = (int)zu4_experience_graphics_theme();
}

Zu4ExperienceProfile zu4_experience_profile(void) {
    return experiencePreferences.profile;
}

const char *zu4_experience_profile_key(Zu4ExperienceProfile profile) {
    switch (profile) {
    case ZU4_EXPERIENCE_CLASSIC: return "classic";
    case ZU4_EXPERIENCE_ASSISTED: return "assisted";
    case ZU4_EXPERIENCE_ULTIMATUM:
    default: return "ultimatum";
    }
}

const char *zu4_experience_profile_name(Zu4ExperienceProfile profile) {
    switch (profile) {
    case ZU4_EXPERIENCE_CLASSIC: return "Classic";
    case ZU4_EXPERIENCE_ASSISTED: return "Assisted";
    case ZU4_EXPERIENCE_ULTIMATUM:
    default: return "Ultimatum";
    }
}

bool zu4_experience_profile_from_key(const char *key, Zu4ExperienceProfile *profile) {
    if (!key || !profile)
        return false;
    if (strcmp(key, "classic") == 0)
        *profile = ZU4_EXPERIENCE_CLASSIC;
    else if (strcmp(key, "ultimatum") == 0)
        *profile = ZU4_EXPERIENCE_ULTIMATUM;
    else if (strcmp(key, "assisted") == 0)
        *profile = ZU4_EXPERIENCE_ASSISTED;
    else
        return false;
    return true;
}

bool zu4_experience_set_profile(Zu4ExperienceProfile profile, SettingsData *target) {
    if (profile < ZU4_EXPERIENCE_CLASSIC || profile > ZU4_EXPERIENCE_ASSISTED)
        return false;
    experiencePreferences.profile = profile;
    experiencePreferences.filterMovementMessagesOverride = -1;
    experiencePreferences.graphicsThemeOverride = -1;
    experiencePreferences.explorationMapOverride = -1;
    experiencePreferences.mapPinsOverride = -1;
    zu4_experience_apply(target);
    return true;
}

bool zu4_experience_is_customized(void) {
    return experiencePreferences.filterMovementMessagesOverride >= 0 ||
           experiencePreferences.graphicsThemeOverride >= 0 ||
           experiencePreferences.explorationMapOverride >= 0 ||
           experiencePreferences.mapPinsOverride >= 0;
}

void zu4_experience_restore_defaults(SettingsData *target) {
    experiencePreferences.filterMovementMessagesOverride = -1;
    experiencePreferences.graphicsThemeOverride = -1;
    experiencePreferences.explorationMapOverride = -1;
    experiencePreferences.mapPinsOverride = -1;
    zu4_experience_apply(target);
}

bool zu4_experience_set_filter_movement_messages(bool enabled, SettingsData *target) {
    experiencePreferences.filterMovementMessagesOverride =
        enabled == profile_filter_default(experiencePreferences.profile) ? -1 : (enabled ? 1 : 0);
    zu4_experience_apply(target);
    return true;
}

bool zu4_experience_set_graphics_theme(Zu4GraphicsTheme theme, SettingsData *target) {
    if (theme != ZU4_GRAPHICS_EGA && theme != ZU4_GRAPHICS_VGA)
        return false;
    experiencePreferences.graphicsThemeOverride =
        theme == profile_graphics_default(experiencePreferences.profile) ? -1 : (int)theme;
    zu4_experience_apply(target);
    return true;
}

bool zu4_experience_set_exploration_map(bool enabled, SettingsData *target) {
    bool profileDefault = profile_default(ZU4_FEATURE_EXPLORATION_MAP,
                                          experiencePreferences.profile) != 0;
    experiencePreferences.explorationMapOverride =
        enabled == profileDefault ? -1 : (enabled ? 1 : 0);
    zu4_experience_apply(target);
    return true;
}

bool zu4_experience_set_map_pins(bool enabled, SettingsData *target) {
    bool profileDefault = profile_default(ZU4_FEATURE_MAP_PINS,
                                          experiencePreferences.profile) != 0;
    experiencePreferences.mapPinsOverride =
        enabled == profileDefault ? -1 : (enabled ? 1 : 0);
    zu4_experience_apply(target);
    return true;
}
