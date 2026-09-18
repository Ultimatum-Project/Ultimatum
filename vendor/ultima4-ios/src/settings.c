/*
 * settings.c
 * Copyright (C) 2013-2016 Darren Janeczek
 * Copyright (C) 2020 R. Danbrook
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

#include "error.h"
#include "experience_settings.h"
#include "settings.h"
#include "soundtrack.h"

#define SETTINGS_BASE_FILENAME "xu4rc"

static u4settings_t u4settings;
SettingsData settings;

static bool parse_int(const char *text, int *value) {
    char *end = NULL;
    long parsed;
    if (!text || !text[0] || !value)
        return false;
    parsed = strtol(text, &end, 10);
    if (!end || *end != '\0')
        return false;
    *value = (int)parsed;
    return true;
}

u4settings_t* zu4_settings_ptr() { return &u4settings; }

void zu4_settings_init(bool useProfile, const char *profileName) {
	if (useProfile) {
		snprintf(u4settings.path, sizeof(u4settings.path), "./profiles/%s/", profileName);
		mkdir("./profiles", S_IRWXU|S_IRWXG|S_IRWXO);
	}
	else {
		char *home = getenv("HOME");
		if (home && home[0]) {
#ifdef ZU4_IOS
			/* The iOS sandbox forbids writing to the container root ($HOME);
			 * only Documents/Library/tmp are writable. Save under Documents. */
			snprintf(u4settings.path, sizeof(u4settings.path), "%s/Documents/.xu4/", home);
#else
			snprintf(u4settings.path, sizeof(u4settings.path), "%s/.xu4/", home);
#endif
		}
		else { snprintf(u4settings.path, sizeof(u4settings.path), "./"); }
	}
	mkdir(u4settings.path, S_IRWXU|S_IRWXG|S_IRWXO);
	snprintf(u4settings.filename, sizeof(u4settings.filename), "%s%s", u4settings.path, SETTINGS_BASE_FILENAME);
	zu4_settings_read();
}

void zu4_settings_setdata(const SettingsData data) {
	settings = data;
}

bool zu4_settings_read() {
    char buffer[256];    
    FILE *settingsFile;
    extern int eventTimerGranularity;   
    bool hasExperienceProfile = false;
    bool hasExperienceProfileKey = false;

    /* default settings */
    settings.scale                 = DEFAULT_SCALE;
    settings.fullscreen            = DEFAULT_FULLSCREEN;
    settings.videoType             = DEFAULT_VIDEO_TYPE;
    settings.gemLayout             = DEFAULT_GEM_LAYOUT;
    settings.lineOfSight           = DEFAULT_LINEOFSIGHT;
    settings.screenShakes          = DEFAULT_SCREEN_SHAKES;
    settings.gamma                 = DEFAULT_GAMMA;
    settings.musicVol              = DEFAULT_MUSIC_VOLUME;
    settings.soundtrack            = ZU4_SOUNDTRACK_HURIN;
    settings.soundVol              = DEFAULT_SOUND_VOLUME;
    settings.volumeFades           = DEFAULT_VOLUME_FADES;
    settings.shortcutCommands      = DEFAULT_SHORTCUT_COMMANDS;
    settings.bumpInteractions      = DEFAULT_BUMP_INTERACTIONS;
    settings.directInteractions    = DEFAULT_DIRECT_INTERACTIONS;
    settings.tapToWalk             = DEFAULT_TAP_TO_WALK;
    settings.dpadSize              = 1;
    settings.flipControls          = false;
    settings.fastCombatPresentation = false;
    settings.keydelay              = DEFAULT_KEY_DELAY;
    settings.keyinterval           = DEFAULT_KEY_INTERVAL;
    settings.filterMoveMessages    = DEFAULT_FILTER_MOVE_MESSAGES;
    settings.battleSpeed           = DEFAULT_BATTLE_SPEED;
    settings.enhancements          = DEFAULT_ENHANCEMENTS;    
    settings.gameCyclesPerSecond   = DEFAULT_CYCLES_PER_SECOND;
    //settings.screenAnimationFramesPerSecond = DEFAULT_ANIMATION_FRAMES_PER_SECOND;
    settings.debug                 = DEFAULT_DEBUG;
    settings.battleDiff            = DEFAULT_BATTLE_DIFFICULTY;
    settings.validateXml           = DEFAULT_VALIDATE_XML;
    settings.spellEffectSpeed      = DEFAULT_SPELL_EFFECT_SPEED;
    settings.campTime              = DEFAULT_CAMP_TIME;
    settings.innTime               = DEFAULT_INN_TIME;
    settings.shrineTime            = DEFAULT_SHRINE_TIME;
    settings.shakeInterval         = DEFAULT_SHAKE_INTERVAL;
    settings.titleSpeedRandom      = DEFAULT_TITLE_SPEED_RANDOM;
    settings.titleSpeedOther       = DEFAULT_TITLE_SPEED_OTHER;

    /* all specific minor enhancements default to "on", any major enhancements default to "off" */
    settings.enhancementsOptions.activePlayer     = true;
    settings.enhancementsOptions.u5spellMixing    = true;
    settings.enhancementsOptions.u5shrines        = true;
    settings.enhancementsOptions.slimeDivides     = false;
    settings.enhancementsOptions.gazerSpawnsInsects = true;
    settings.enhancementsOptions.textColorization = false;
    settings.enhancementsOptions.c64chestTraps    = true;
    settings.enhancementsOptions.smartEnterKey    = true;
    settings.enhancementsOptions.peerShowsObjects = false;
    settings.enhancementsOptions.u5combat         = false;

    settings.innAlwaysCombat = 0;
    settings.campingAlwaysCombat = 0;
    zu4_experience_reset_new_install();
    
    settingsFile = fopen(u4settings.filename, "rt");    
    if (!settingsFile) {
        zu4_experience_apply(&settings);
        eventTimerGranularity = (1000 / settings.gameCyclesPerSecond);
        return false;
    }

    while(fgets(buffer, sizeof(buffer), settingsFile) != NULL) {
        while (strlen(buffer) > 0 && isspace((unsigned char)buffer[strlen(buffer) - 1]))
            buffer[strlen(buffer) - 1] = '\0';

        if (strstr(buffer, "experienceSchemaVersion=") == buffer) {
            if (!parse_int(buffer + strlen("experienceSchemaVersion="), &experiencePreferences.schemaVersion))
                experiencePreferences.schemaVersion = 0;
        }
        else if (strstr(buffer, "experienceProfile=") == buffer) {
            Zu4ExperienceProfile profile;
            hasExperienceProfileKey = true;
            hasExperienceProfile = zu4_experience_profile_from_key(
                buffer + strlen("experienceProfile="), &profile);
            if (hasExperienceProfile)
                experiencePreferences.profile = profile;
        }
        else if (strstr(buffer, "experienceOverride.filterMovementMessages=") == buffer) {
            if (!parse_int(buffer + strlen("experienceOverride.filterMovementMessages="),
                           &experiencePreferences.filterMovementMessagesOverride))
                experiencePreferences.filterMovementMessagesOverride = -2;
        }
        else if (strstr(buffer, "experienceOverride.graphicsTheme=") == buffer) {
            if (!parse_int(buffer + strlen("experienceOverride.graphicsTheme="),
                           &experiencePreferences.graphicsThemeOverride))
                experiencePreferences.graphicsThemeOverride = -2;
        }
        else if (strstr(buffer, "experienceOverride.explorationMap=") == buffer) {
            if (!parse_int(buffer + strlen("experienceOverride.explorationMap="),
                           &experiencePreferences.explorationMapOverride))
                experiencePreferences.explorationMapOverride = -2;
        }
        else if (strstr(buffer, "experienceOverride.mapPins=") == buffer) {
            if (!parse_int(buffer + strlen("experienceOverride.mapPins="),
                           &experiencePreferences.mapPinsOverride))
                experiencePreferences.mapPinsOverride = -2;
        }
        else if (strstr(buffer, "scale=") == buffer)
            settings.scale = (unsigned int) strtoul(buffer + strlen("scale="), NULL, 0);
        else if (strstr(buffer, "fullscreen=") == buffer)
            settings.fullscreen = (int) strtoul(buffer + strlen("fullscreen="), NULL, 0);
        else if (strstr(buffer, "video=") == buffer)
            settings.videoType = (int) strtoul(buffer + strlen("video="), NULL, 0);
        else if (strstr(buffer, "gemLayout=") == buffer)
            settings.gemLayout = (int) strtoul(buffer + strlen("gemLayout="), NULL, 0);
        else if (strstr(buffer, "lineOfSight=") == buffer)
            settings.lineOfSight = (int) strtoul(buffer + strlen("lineOfSight="), NULL, 0);
        else if (strstr(buffer, "screenShakes=") == buffer)
            settings.screenShakes = (int) strtoul(buffer + strlen("screenShakes="), NULL, 0);        
        else if (strstr(buffer, "gamma=") == buffer)
            settings.gamma = (int) strtoul(buffer + strlen("gamma="), NULL, 0);        
        else if (strstr(buffer, "audio.soundtrack=") == buffer) {
            int id = zu4_soundtrack_from_key(buffer + strlen("audio.soundtrack="));
            if (id >= 0) settings.soundtrack = id;
        }
        else if (strstr(buffer, "musicVol=") == buffer)
            settings.musicVol = (int) strtoul(buffer + strlen("musicVol="), NULL, 0);
        else if (strstr(buffer, "soundVol=") == buffer)
            settings.soundVol = (int) strtoul(buffer + strlen("soundVol="), NULL, 0);
        else if (strstr(buffer, "volumeFades=") == buffer)
            settings.volumeFades = (int) strtoul(buffer + strlen("volumeFades="), NULL, 0);        
        else if (strstr(buffer, "shortcutCommands=") == buffer)
            settings.shortcutCommands = (int) strtoul(buffer + strlen("shortcutCommands="), NULL, 0);
        else if (strstr(buffer, "bumpInteractions=") == buffer)
            settings.bumpInteractions = (int) strtoul(buffer + strlen("bumpInteractions="), NULL, 0);
        else if (strstr(buffer, "directInteractions=") == buffer)
            settings.directInteractions = (int) strtoul(buffer + strlen("directInteractions="), NULL, 0);
        else if (strstr(buffer, "tapToWalk=") == buffer)
            settings.tapToWalk = (int) strtoul(buffer + strlen("tapToWalk="), NULL, 0);
        else if (strstr(buffer, "dpadSize=") == buffer) {
            int value = -1;
            if (parse_int(buffer + strlen("dpadSize="), &value) && value >= 0 && value <= 2)
                settings.dpadSize = value;
        }
        else if (strstr(buffer, "flipControls=") == buffer) {
            int value = -1;
            if (parse_int(buffer + strlen("flipControls="), &value) && (value == 0 || value == 1))
                settings.flipControls = value != 0;
        }
        else if (strstr(buffer, "fastCombatPresentation=") == buffer) {
            int value = -1;
            if (parse_int(buffer + strlen("fastCombatPresentation="), &value) && (value == 0 || value == 1))
                settings.fastCombatPresentation = value != 0;
        }
        else if (strstr(buffer, "keydelay=") == buffer)
            settings.keydelay = (int) strtoul(buffer + strlen("keydelay="), NULL, 0);
        else if (strstr(buffer, "keyinterval=") == buffer)
            settings.keyinterval = (int) strtoul(buffer + strlen("keyinterval="), NULL, 0);
        else if (strstr(buffer, "filterMoveMessages=") == buffer)
            settings.filterMoveMessages = (int) strtoul(buffer + strlen("filterMoveMessages="), NULL, 0);
        else if (strstr(buffer, "battlespeed=") == buffer)
            settings.battleSpeed = (int) strtoul(buffer + strlen("battlespeed="), NULL, 0);
        else if (strstr(buffer, "enhancements=") == buffer)
            settings.enhancements = (int) strtoul(buffer + strlen("enhancements="), NULL, 0);        
        else if (strstr(buffer, "gameCyclesPerSecond=") == buffer)
            settings.gameCyclesPerSecond = (int) strtoul(buffer + strlen("gameCyclesPerSecond="), NULL, 0);
        else if (strstr(buffer, "debug=") == buffer)
            settings.debug = (int) strtoul(buffer + strlen("debug="), NULL, 0);
        else if (strstr(buffer, "battleDiff=") == buffer)
            settings.battleDiff = (int) strtoul(buffer + strlen("battleDiff="), NULL, 0);
        else if (strstr(buffer, "validateXml=") == buffer)
            settings.validateXml = (int) strtoul(buffer + strlen("validateXml="), NULL, 0);
        else if (strstr(buffer, "spellEffectSpeed=") == buffer)
            settings.spellEffectSpeed = (int) strtoul(buffer + strlen("spellEffectSpeed="), NULL, 0);
        else if (strstr(buffer, "campTime=") == buffer)
            settings.campTime = (int) strtoul(buffer + strlen("campTime="), NULL, 0);
        else if (strstr(buffer, "innTime=") == buffer)
            settings.innTime = (int) strtoul(buffer + strlen("innTime="), NULL, 0);
        else if (strstr(buffer, "shrineTime=") == buffer)
            settings.shrineTime = (int) strtoul(buffer + strlen("shrineTime="), NULL, 0);
        else if (strstr(buffer, "shakeInterval=") == buffer)
            settings.shakeInterval = (int) strtoul(buffer + strlen("shakeInterval="), NULL, 0);
        else if (strstr(buffer, "titleSpeedRandom=") == buffer)
            settings.titleSpeedRandom = (int) strtoul(buffer + strlen("titleSpeedRandom="), NULL, 0);
        else if (strstr(buffer, "titleSpeedOther=") == buffer)
            settings.titleSpeedOther = (int) strtoul(buffer + strlen("titleSpeedOther="), NULL, 0);
        
        /* minor enhancement options */
        else if (strstr(buffer, "activePlayer=") == buffer)
            settings.enhancementsOptions.activePlayer = (int) strtoul(buffer + strlen("activePlayer="), NULL, 0);
        else if (strstr(buffer, "u5spellMixing=") == buffer)
            settings.enhancementsOptions.u5spellMixing = (int) strtoul(buffer + strlen("u5spellMixing="), NULL, 0);
        else if (strstr(buffer, "u5shrines=") == buffer)
            settings.enhancementsOptions.u5shrines = (int) strtoul(buffer + strlen("u5shrines="), NULL, 0);
        else if (strstr(buffer, "slimeDivides=") == buffer)
            settings.enhancementsOptions.slimeDivides = (int) strtoul(buffer + strlen("slimeDivides="), NULL, 0);
        else if (strstr(buffer, "gazerSpawnsInsects=") == buffer)
            settings.enhancementsOptions.gazerSpawnsInsects = (int) strtoul(buffer + strlen("gazerSpawnsInsects="), NULL, 0);
        else if (strstr(buffer, "textColorization=") == buffer)
            settings.enhancementsOptions.textColorization = (int) strtoul(buffer + strlen("textColorization="), NULL, 0);
        else if (strstr(buffer, "c64chestTraps=") == buffer)
            settings.enhancementsOptions.c64chestTraps = (int) strtoul(buffer + strlen("c64chestTraps="), NULL, 0);                
        else if (strstr(buffer, "smartEnterKey=") == buffer)
            settings.enhancementsOptions.smartEnterKey = (int) strtoul(buffer + strlen("smartEnterKey="), NULL, 0);        
        
        /* major enhancement options */
        else if (strstr(buffer, "peerShowsObjects=") == buffer)
            settings.enhancementsOptions.peerShowsObjects = (int) strtoul(buffer + strlen("peerShowsObjects="), NULL, 0);
        else if (strstr(buffer, "u5combat=") == buffer)
            settings.enhancementsOptions.u5combat = (int) strtoul(buffer + strlen("u5combat="), NULL, 0);
        else if (strstr(buffer, "innAlwaysCombat=") == buffer)
            settings.innAlwaysCombat = (int) strtoul(buffer + strlen("innAlwaysCombat="), NULL, 0);
        else if (strstr(buffer, "campingAlwaysCombat=") == buffer)
            settings.campingAlwaysCombat = (int) strtoul(buffer + strlen("campingAlwaysCombat="), NULL, 0);    

        else
            zu4_error(ZU4_LOG_WRN, "invalid line in settings file %s", buffer);
    }

    fclose(settingsFile);

    if (!hasExperienceProfileKey)
        zu4_experience_migrate_legacy(true, settings.filterMoveMessages, settings.videoType);
    else if (!hasExperienceProfile) {
        experiencePreferences.profile = ZU4_EXPERIENCE_ULTIMATUM;
        experiencePreferences.filterMovementMessagesOverride = -1;
        experiencePreferences.graphicsThemeOverride = -1;
        experiencePreferences.explorationMapOverride = -1;
        experiencePreferences.mapPinsOverride = -1;
    }
    zu4_experience_validate();
    zu4_experience_apply(&settings);

    eventTimerGranularity = (1000 / settings.gameCyclesPerSecond);
    return true;
}

bool zu4_settings_write() {    
    FILE *settingsFile;
    char temporaryFilename[sizeof(u4settings.filename) + 8];
    bool succeeded;
        
    snprintf(temporaryFilename, sizeof(temporaryFilename), "%s.tmp", u4settings.filename);
    settingsFile = fopen(temporaryFilename, "wt");
    if (!settingsFile) {
        zu4_error(ZU4_LOG_WRN, "can't write settings file");
        return false;
    }    

    fprintf(settingsFile, 
            "experienceSchemaVersion=%d\n"
            "experienceProfile=%s\n"
            "experienceOverride.filterMovementMessages=%d\n"
            "experienceOverride.graphicsTheme=%d\n"
            "experienceOverride.explorationMap=%d\n"
            "experienceOverride.mapPins=%d\n"
            "scale=%d\n"
            "fullscreen=%d\n"
            "video=%d\n"
            "gemLayout=%d\n"
            "lineOfSight=%d\n"
            "screenShakes=%d\n"
            "gamma=%d\n"
            "musicVol=%d\n"
            "audio.soundtrack=%s\n"
            "soundVol=%d\n"
            "volumeFades=%d\n"
            "shortcutCommands=%d\n"
            "bumpInteractions=%d\n"
            "directInteractions=%d\n"
            "tapToWalk=%d\n"
            "dpadSize=%d\n"
            "flipControls=%d\n"
            "fastCombatPresentation=%d\n"
            "keydelay=%d\n"
            "keyinterval=%d\n"
            "filterMoveMessages=%d\n"
            "battlespeed=%d\n"
            "enhancements=%d\n"            
            "gameCyclesPerSecond=%d\n"
            "debug=%d\n"
            "battleDiff=%d\n"
            "validateXml=%d\n"
            "spellEffectSpeed=%d\n"
            "campTime=%d\n"
            "innTime=%d\n"
            "shrineTime=%d\n"
            "shakeInterval=%d\n"
            "titleSpeedRandom=%d\n"
            "titleSpeedOther=%d\n"
            "activePlayer=%d\n"
            "u5spellMixing=%d\n"
            "u5shrines=%d\n"
            "slimeDivides=%d\n"
            "gazerSpawnsInsects=%d\n"
            "textColorization=%d\n"
            "c64chestTraps=%d\n"            
            "smartEnterKey=%d\n"
            "peerShowsObjects=%d\n"
            "u5combat=%d\n"
            "innAlwaysCombat=%d\n"
            "campingAlwaysCombat=%d\n",
            experiencePreferences.schemaVersion,
            zu4_experience_profile_key(experiencePreferences.profile),
            experiencePreferences.filterMovementMessagesOverride,
            experiencePreferences.graphicsThemeOverride,
            experiencePreferences.explorationMapOverride,
            experiencePreferences.mapPinsOverride,
            settings.scale,
            settings.fullscreen,
            settings.videoType,
            settings.gemLayout,
            settings.lineOfSight,
            settings.screenShakes,
            settings.gamma,
            settings.musicVol,
            zu4_soundtrack(settings.soundtrack) ? zu4_soundtrack(settings.soundtrack)->key : "hurin",
            settings.soundVol,
            settings.volumeFades,
            settings.shortcutCommands,
            settings.bumpInteractions,
            settings.directInteractions,
            settings.tapToWalk,
            settings.dpadSize,
            settings.flipControls,
            settings.fastCombatPresentation,
            settings.keydelay,
            settings.keyinterval,
            settings.filterMoveMessages,
            settings.battleSpeed,
            settings.enhancements,            
            settings.gameCyclesPerSecond,
            settings.debug,
            settings.battleDiff,
            settings.validateXml,
            settings.spellEffectSpeed,
            settings.campTime,
            settings.innTime,
            settings.shrineTime,
            settings.shakeInterval,
            settings.titleSpeedRandom,
            settings.titleSpeedOther,
            settings.enhancementsOptions.activePlayer,
            settings.enhancementsOptions.u5spellMixing,
            settings.enhancementsOptions.u5shrines,
            settings.enhancementsOptions.slimeDivides,
            settings.enhancementsOptions.gazerSpawnsInsects,
            settings.enhancementsOptions.textColorization,
            settings.enhancementsOptions.c64chestTraps,            
            settings.enhancementsOptions.smartEnterKey,
            settings.enhancementsOptions.peerShowsObjects,
            settings.enhancementsOptions.u5combat,
            settings.innAlwaysCombat,
            settings.campingAlwaysCombat);

    succeeded = !ferror(settingsFile) && fflush(settingsFile) == 0;
    if (succeeded)
        succeeded = fsync(fileno(settingsFile)) == 0;
    if (fclose(settingsFile) != 0)
        succeeded = false;
    if (succeeded)
        succeeded = rename(temporaryFilename, u4settings.filename) == 0;
    if (!succeeded) {
        unlink(temporaryFilename);
        zu4_error(ZU4_LOG_WRN, "can't atomically replace settings file");
    }
    return succeeded;
}
