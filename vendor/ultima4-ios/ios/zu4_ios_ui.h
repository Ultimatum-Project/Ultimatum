/*
 *  zu4_ios_ui.h
 *  On-screen touch controls for the iOS port (native UIKit button overlay).
 */
#ifndef __zu4_ios_ui_h__
#define __zu4_ios_ui_h__

#include "SDL.h"

#define ZU4_IOS_ACTION_EVENT 0x5A34

enum Zu4MobileAction {
    ZU4_MOBILE_ACTION_MENU = 1,
    ZU4_MOBILE_ACTION_CAST,
    ZU4_MOBILE_ACTION_PARTY,
    ZU4_MOBILE_ACTION_JOURNAL,
    ZU4_MOBILE_ACTION_ENTER,
    ZU4_MOBILE_ACTION_TALK,
    ZU4_MOBILE_ACTION_PARTY_MEMBER,
    ZU4_MOBILE_ACTION_MAP,
    ZU4_MOBILE_ACTION_CONTEXT,
    ZU4_MOBILE_ACTION_MOVE,
    ZU4_MOBILE_ACTION_ADJACENT,
    ZU4_MOBILE_ACTION_DUNGEON_SEARCH,
    ZU4_MOBILE_ACTION_DUNGEON_TORCH,
    ZU4_MOBILE_ACTION_DUNGEON_VIEW,
    ZU4_MOBILE_ACTION_COMBAT_TARGET,
    ZU4_MOBILE_ACTION_COMBAT_CYCLE,
    ZU4_MOBILE_ACTION_COMBAT_CLEAR_TARGET,
    ZU4_MOBILE_ACTION_WALK_START,
    ZU4_MOBILE_ACTION_WALK_STEP,
    ZU4_MOBILE_ACTION_COMBAT_REPEAT_ATTACK
};

#define ZU4_MOBILE_MAX_PARTY_MEMBERS 8
#define ZU4_MOBILE_PARTY_NAME_CAPACITY 32

typedef struct Zu4MobilePartyMemberStatus {
    char name[ZU4_MOBILE_PARTY_NAME_CAPACITY];
    int hp;
    int maxHp;
    char condition;
    int active;
} Zu4MobilePartyMemberStatus;

#define ZU4_MOBILE_MAX_COMBAT_TARGETS 16
#define ZU4_MOBILE_COMBAT_TARGET_NAME_CAPACITY 64
typedef struct Zu4MobileCombatTarget {
    int token;
    int screenX;
    int screenY;
    int attackerScreenX;
    int attackerScreenY;
    int distance;
    int direction;
    int selected;
    char name[ZU4_MOBILE_COMBAT_TARGET_NAME_CAPACITY];
} Zu4MobileCombatTarget;

#define ZU4_MOBILE_MAP_PIN_LABEL_CAPACITY 161
typedef struct Zu4MobileMapPin {
    int x;
    int y;
    char label[ZU4_MOBILE_MAP_PIN_LABEL_CAPACITY];
} Zu4MobileMapPin;

#define ZU4_MOBILE_MAP_DISCOVERY_NAME_CAPACITY 161
enum Zu4MobileMapDiscoveryCategory {
    ZU4_MOBILE_MAP_DISCOVERY_TOWN = 1,
    ZU4_MOBILE_MAP_DISCOVERY_CASTLE,
    ZU4_MOBILE_MAP_DISCOVERY_VILLAGE,
    ZU4_MOBILE_MAP_DISCOVERY_SHRINE,
    ZU4_MOBILE_MAP_DISCOVERY_DUNGEON
};
typedef struct Zu4MobileMapDiscovery {
    int x;
    int y;
    int category;
    char name[ZU4_MOBILE_MAP_DISCOVERY_NAME_CAPACITY];
} Zu4MobileMapDiscovery;

typedef void (*Zu4MapDismiss)(void *context);

#ifdef __cplusplus
extern "C" {
#endif

/* Add the on-screen control buttons (D-pad + action keys) on top of the SDL
 * view for the given window. Safe to call more than once; only installs once. */
void zu4_ios_setup_ui(SDL_Window *window);

/* Show/hide (or toggle) the iOS on-screen keyboard. */
void zu4_ios_show_keyboard(int show);
void zu4_ios_toggle_keyboard(void);
int zu4_ios_title_rect(SDL_Rect *rect, int pixelWidth, int pixelHeight);
void zu4_ios_refresh_controls(void);
void zu4_ios_set_native_text_input_active(int active);

/* Semantic command, accepted only by the active exploration controller. */
void zu4_mobile_talk(void);
void zu4_mobile_enter(void);
void zu4_mobile_context(void);
void zu4_mobile_move(int direction, int allowBump);
void zu4_mobile_adjacent_interaction(int encoded);
int zu4_mobile_capture_adjacent_interaction(int direction);
int zu4_mobile_capture_walk(int offsetX, int offsetY);
void zu4_mobile_cancel_walk(void);
void zu4_mobile_set_bump_input_eligible(int eligible);
void zu4_mobile_journal(void);
void zu4_mobile_party(void);
void zu4_mobile_party_member(int index);
void zu4_mobile_exploration_map(void);
int zu4_mobile_prepare_exploration_map(void);
const unsigned char *zu4_mobile_prepared_map_pixels(void);
int zu4_mobile_prepared_map_width(void);
int zu4_mobile_prepared_map_height(void);
int zu4_mobile_prepared_map_player_x(void);
int zu4_mobile_prepared_map_player_y(void);
void zu4_mobile_cast(void);
int zu4_mobile_combat_active(void);
int zu4_mobile_combat_target_count(void);
int zu4_mobile_combat_target_at(int index, Zu4MobileCombatTarget *target);
int zu4_mobile_combat_target_selected(void);
int zu4_mobile_combat_target_prepared(void);
int zu4_mobile_combat_repeat_target(char *name, int capacity);
int zu4_mobile_dungeon_active(void);
int zu4_mobile_dungeon_top_down(void);
int zu4_mobile_dungeon_cell_revealed(int mapId, int x, int y, int level);
int zu4_mobile_dungeon_remember_cell(int mapId, int x, int y, int level);
void zu4_mobile_menu(void);
void zu4_mobile_perform_action(int action, int parameter);
/* Shared engine-semantic subset used by non-UIKit hosts. */
void zu4_mobile_perform_gameplay_action(int action, int parameter);
void zu4_ios_message(const char *text);
int zu4_mobile_map_visible(void);
int zu4_mobile_can_repeat_movement(void);
int zu4_mobile_world_status(char *buffer, int capacity);
int zu4_mobile_party_status(Zu4MobilePartyMemberStatus *members, int capacity);
int zu4_mobile_exploration_map_enabled(void);
int zu4_mobile_minimap(unsigned char *rgba, int side);
int zu4_mobile_map_pins_enabled(void);
int zu4_mobile_map_pin_count(void);
int zu4_mobile_map_pin_at(int index, Zu4MobileMapPin *pin);
int zu4_mobile_map_discovery_count(void);
int zu4_mobile_map_discovery_at(int index, Zu4MobileMapDiscovery *place);
int zu4_mobile_can_set_map_pin(int x, int y);
int zu4_mobile_set_map_pin(int x, int y, const char *label);
int zu4_mobile_remove_map_pin(int x, int y);
int zu4_ios_native_text_input_active(void);
/* SDL lifecycle bridge. Background saves are synchronous and transactional. */
void zu4_mobile_lifecycle_background(void);
void zu4_mobile_lifecycle_foreground(void);
int zu4_mobile_context_action(char *label, int capacity);
int zu4_mobile_direct_interactions_enabled(void);
int zu4_mobile_world_taps_enabled(void);
void zu4_ios_show_exploration_map(const unsigned char *rgba, int width, int height,
                                  int playerX, int playerY, int pinsEnabled);
void zu4_ios_show_dungeon_exploration_map(const unsigned char *rgba, int width, int height,
                                          int playerX, int playerY,
                                          const char *dungeonName, int level);
void zu4_ios_show_gem_map(const unsigned char *rgba, int width, int height,
                          int playerX, int playerY, const char *locationName,
                          Zu4MapDismiss dismiss, void *context);
void zu4_ios_dismiss_map(void);
int zu4_ios_world_rect(SDL_Rect *rect, int pixelWidth, int pixelHeight, const char *status);

#ifdef __cplusplus
}
#endif

#endif /* __zu4_ios_ui_h__ */
