#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "experience_settings.h"
#include "settings.h"
#include "soundtrack.h"

int eventTimerGranularity = 0;

static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wt");
    assert(file);
    assert(fputs(text, file) >= 0);
    assert(fclose(file) == 0);
}

static void test_profile_defaults_and_overrides(void) {
    SettingsData data = {0};
    assert(zu4_experience_feature_count() == 4);
    assert(strcmp(zu4_experience_feature_definition(0)->stableKey,
                  "presentation.filterMovementMessages") == 0);
    assert(zu4_experience_feature_definition(1)->applyPolicy == ZU4_APPLY_IMMEDIATE);
    assert(strcmp(zu4_experience_feature_definition(2)->stableKey,
                  "navigation.explorationMap") == 0);
    assert(strcmp(zu4_experience_feature_definition(3)->stableKey,
                  "navigation.playerMapPins") == 0);
    assert(zu4_experience_feature_definition(3)->classification == ZU4_FEATURE_CONVENIENCE);
    assert(zu4_experience_feature_definition(4) == NULL);
    zu4_experience_reset_new_install();
    zu4_experience_apply(&data);
    assert(zu4_experience_profile() == ZU4_EXPERIENCE_ULTIMATUM);
    assert(zu4_experience_filter_movement_messages());
    assert(zu4_experience_graphics_theme() == ZU4_GRAPHICS_VGA);
    assert(zu4_experience_exploration_map());
    assert(zu4_experience_map_pins());
    assert(!zu4_experience_is_customized());

    assert(zu4_experience_set_graphics_theme(ZU4_GRAPHICS_EGA, &data));
    assert(data.videoType == ZU4_GRAPHICS_EGA);
    assert(zu4_experience_exploration_map());
    assert(zu4_experience_is_customized());
    zu4_experience_restore_defaults(&data);
    assert(data.videoType == ZU4_GRAPHICS_VGA);
    assert(!zu4_experience_is_customized());

    assert(zu4_experience_set_profile(ZU4_EXPERIENCE_CLASSIC, &data));
    assert(!data.filterMoveMessages);
    assert(data.videoType == ZU4_GRAPHICS_EGA);
    assert(!zu4_experience_exploration_map());
    assert(!zu4_experience_map_pins());
    assert(zu4_experience_set_filter_movement_messages(true, &data));
    assert(data.filterMoveMessages);
    assert(zu4_experience_is_customized());
    assert(zu4_experience_set_exploration_map(true, &data));
    assert(zu4_experience_exploration_map());
    assert(zu4_experience_set_map_pins(true, &data));
    assert(zu4_experience_map_pins());

    assert(zu4_experience_set_profile(ZU4_EXPERIENCE_ASSISTED, &data));
    assert(data.filterMoveMessages);
    assert(data.videoType == ZU4_GRAPHICS_VGA);
    assert(zu4_experience_exploration_map());
    assert(zu4_experience_map_pins());
    assert(!zu4_experience_is_customized());
}

static void test_legacy_migration(void) {
    SettingsData data = {0};
    zu4_experience_reset_new_install();
    zu4_experience_migrate_legacy(true, 0, ZU4_GRAPHICS_EGA);
    zu4_experience_apply(&data);
    assert(zu4_experience_profile() == ZU4_EXPERIENCE_ULTIMATUM);
    assert(zu4_experience_is_customized());
    assert(!data.filterMoveMessages);
    assert(data.videoType == ZU4_GRAPHICS_EGA);
}

static void test_validation(void) {
    SettingsData data = {0};
    zu4_experience_reset_new_install();
    experiencePreferences.profile = (Zu4ExperienceProfile)99;
    experiencePreferences.filterMovementMessagesOverride = 12;
    experiencePreferences.graphicsThemeOverride = -8;
    experiencePreferences.explorationMapOverride = 7;
    experiencePreferences.mapPinsOverride = -9;
    assert(!zu4_experience_validate());
    zu4_experience_apply(&data);
    assert(zu4_experience_profile() == ZU4_EXPERIENCE_ULTIMATUM);
    assert(!zu4_experience_is_customized());
    assert(data.filterMoveMessages);
    assert(data.videoType == ZU4_GRAPHICS_VGA);
    assert(zu4_experience_map_pins());
}

static void test_atomic_persistence(void) {
    char directory[] = "/private/tmp/zu4-experience.XXXXXX";
    char original[1024];
    assert(getcwd(original, sizeof(original)));
    assert(mkdtemp(directory));
    assert(chdir(directory) == 0);

    zu4_settings_init(true, "test");
    assert(settings.soundtrack == ZU4_SOUNDTRACK_HURIN);
    assert(zu4_experience_profile() == ZU4_EXPERIENCE_ULTIMATUM);

    write_text("profiles/test/xu4rc",
        "video=0\nfilterMoveMessages=0\ngameCyclesPerSecond=4\n");
    assert(zu4_settings_read());
    assert(zu4_experience_profile() == ZU4_EXPERIENCE_ULTIMATUM);
    assert(zu4_experience_is_customized());
    assert(!settings.filterMoveMessages);
    assert(settings.videoType == ZU4_GRAPHICS_EGA);

    assert(zu4_experience_set_profile(ZU4_EXPERIENCE_ULTIMATUM, &settings));
    assert(zu4_experience_set_graphics_theme(ZU4_GRAPHICS_EGA, &settings));
    assert(zu4_experience_set_exploration_map(false, &settings));
    assert(zu4_experience_set_map_pins(false, &settings));
    settings.tapToWalk = true;
    settings.dpadSize = 2;
    settings.flipControls = true;
    settings.fastCombatPresentation = true;
    settings.soundtrack = ZU4_SOUNDTRACK_HURIN;
    assert(zu4_settings_write());

    zu4_experience_reset_new_install();
    settings.videoType = ZU4_GRAPHICS_VGA;
    settings.tapToWalk = false;
    settings.dpadSize = 0;
    settings.flipControls = false;
    settings.fastCombatPresentation = false;
    settings.soundtrack = ZU4_SOUNDTRACK_HURIN;
    assert(zu4_settings_read());
    assert(zu4_experience_profile() == ZU4_EXPERIENCE_ULTIMATUM);
    assert(zu4_experience_is_customized());
    assert(settings.videoType == ZU4_GRAPHICS_EGA);
    assert(!zu4_experience_exploration_map());
    assert(!zu4_experience_map_pins());
    assert(settings.tapToWalk);
    assert(settings.dpadSize == 2);
    assert(settings.flipControls);
    assert(settings.fastCombatPresentation);
    assert(settings.soundtrack == ZU4_SOUNDTRACK_HURIN);
    assert(zu4_experience_set_profile(ZU4_EXPERIENCE_CLASSIC, &settings));
    zu4_experience_restore_defaults(&settings);
    assert(settings.soundtrack == ZU4_SOUNDTRACK_HURIN);
    assert(settings.dpadSize == 2 && settings.flipControls);
    assert(settings.fastCombatPresentation);
    assert(access("profiles/test/xu4rc.tmp", F_OK) != 0);

    write_text("profiles/test/xu4rc",
        "experienceSchemaVersion=not-a-number\n"
        "experienceProfile=unknown\n"
        "experienceOverride.filterMovementMessages=wrong\n"
        "experienceOverride.graphicsTheme=9\n"
        "dpadSize=9\nflipControls=not-a-number\n"
        "fastCombatPresentation=2\n"
        "audio.soundtrack=invalid\n"
        "gameCyclesPerSecond=4\n");
    assert(zu4_settings_read());
    assert(zu4_experience_profile() == ZU4_EXPERIENCE_ULTIMATUM);
    assert(!zu4_experience_is_customized());
    assert(settings.dpadSize == 1 && !settings.flipControls);
    assert(!settings.fastCombatPresentation);
    assert(settings.soundtrack == ZU4_SOUNDTRACK_HURIN);
    assert(settings.filterMoveMessages);
    assert(settings.videoType == ZU4_GRAPHICS_VGA);
    assert(zu4_experience_exploration_map());
    assert(zu4_experience_map_pins());

    assert(chdir(original) == 0);
    {
        char settingsPath[1200];
        char profilePath[1200];
        snprintf(settingsPath, sizeof(settingsPath), "%s/profiles/test/xu4rc", directory);
        snprintf(profilePath, sizeof(profilePath), "%s/profiles/test", directory);
        assert(unlink(settingsPath) == 0);
        assert(rmdir(profilePath) == 0);
        snprintf(profilePath, sizeof(profilePath), "%s/profiles", directory);
        assert(rmdir(profilePath) == 0);
        assert(rmdir(directory) == 0);
    }
}

int main(void) {
    test_profile_defaults_and_overrides();
    test_legacy_migration();
    test_validation();
    test_atomic_persistence();
    return 0;
}
