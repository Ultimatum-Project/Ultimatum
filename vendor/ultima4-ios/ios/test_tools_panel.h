#ifndef ZU4_TEST_TOOLS_PANEL_H
#define ZU4_TEST_TOOLS_PANEL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Zu4TestToolsState {
    const char *page;
    int activeSlot;
    const char *location;
    int x;
    int y;
    int z;
    int worldMap;
    int dungeon;
    int combat;
    int canDungeonTeleport;
    int collisionOverride;
    int seeThroughWalls;
    int windLocked;
    const char *windDirection;
    int torchDuration;
} Zu4TestToolsState;

typedef void (*Zu4TestToolsSubmit)(const char *action, int enabled, void *context);

int zu4_test_tools_available(void);
void zu4_test_tools_panel_show(const Zu4TestToolsState *state,
                               Zu4TestToolsSubmit submit, void *context);
void zu4_test_tools_panel_dismiss(void);

#ifdef __cplusplus
}
#endif
#endif
