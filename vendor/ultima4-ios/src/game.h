/*
 * $Id: game.h 2938 2011-07-09 18:17:54Z twschulz $
 */

#ifndef GAME_H
#define GAME_H

#include <vector>

#include "event.h"
#include "map.h"
#include "observer.h"
#include "tileview.h"

struct Map;
struct Portal;
struct Creature;
struct Location;
struct MoveEvent;
struct Party;
struct PartyEvent;
struct PartyMember;

typedef enum {
    VIEW_NORMAL,
    VIEW_GEM,
    VIEW_RUNE,
    VIEW_DUNGEON,
    VIEW_DEAD,
    VIEW_CODEX,
    VIEW_MIXTURES
} ViewMode;

// Temporary function that returns a C string directly FIXME
const char *gameGetInputC();

/**
 * A controller to read a player number.
 */
struct ReadPlayerController : public ReadChoiceController {
public:
    ReadPlayerController();
    ~ReadPlayerController();
    virtual bool keyPressed(int key);

    int getPlayer();
    int waitFor();
};

/**
 * A controller to handle input for commands requiring a letter
 * argument in the range 'a' - lastValidLetter.
 */
struct AlphaActionController : public WaitableController<int> {
public:
    AlphaActionController(char letter, const std::string &p) : lastValidLetter(letter), prompt(p) {}
    bool keyPressed(int key);

    static int get(char lastValidLetter, const std::string &prompt, EventHandler *eh = NULL);

private:
    char lastValidLetter;
    std::string prompt;
};

/**
 * Controls interaction while Ztats are being displayed.
 */
struct ZtatsController : public WaitableController<void *> {
public:
    bool keyPressed(int key);
};

struct TurnCompleter {
public:
    virtual ~TurnCompleter() {}
    virtual void finishTurn() = 0;
};

/**
 * The main game controller that handles basic game flow and keypresses.
 *
 * @todo
 *  <ul>
 *      <li>separate the dungeon specific stuff into another struct (subclass?)</li>
 *  </ul>
 */
struct GameController : public Controller, public Observer<Party *, PartyEvent &>, public Observer<Location *, MoveEvent &>,
    public TurnCompleter {
public:
    GameController();

    /* controller functions */
    virtual bool keyPressed(int key);
    virtual void timerFired();

    /* main game functions */
    void init();
    void deinit();
    void initScreen();
    void initScreenWithoutReloadingState();
    void setMap(Map *map, bool saveLocation, const Portal *portal, TurnCompleter *turnCompleter = NULL);
    int exitToParentMap();
    virtual void finishTurn();

    virtual void update(Party *party, PartyEvent &event);
    virtual void update(Location *location, MoveEvent &event);

    void initMoons();
    void updateMoons(bool showmoongates);

    static void flashTile(const Coords &coords, MapTile tile, int timeFactor);
    static void flashTile(const Coords &coords, const std::string &tilename, int timeFactor);
    static void doScreenAnimationsWhilePausing(int timeFactor);

    TileView mapArea;
    bool paused;
    int pausedTimer;

private:
    void avatarMoved(MoveEvent &event);
    void avatarMovedInDungeon(MoveEvent &event);

    void creatureCleanup();
    void checkBridgeTrolls();
    void checkRandomCreatures();
    void checkSpecialCreatures(Direction dir);
    bool checkMoongates();

    bool createBalloon(Map *map);
};

extern GameController *game;

/* map and screen functions */
void gameSetViewMode(ViewMode newMode);
void gameUpdateScreen(void);

/* spell functions */
// Returns true only when a cast was attempted or a mixture was made.
bool castSpell(int player = -1);
void gameSpellEffect(int spell, int player, int sound);

/* action functions */
void destroy();
void attack();
void board();
void fire();
void getChest(int player = -1);
void holeUp();
void jimmy();
void opendoor();
bool gamePeerCity(int city, void *data);
void peer(bool useGem = true);
void talk();
bool fireAt(const Coords &coords, bool originAvatar);
Direction gameGetDirection(const char *prompt = "Choose a direction");
std::string gameGetStoneInput();
std::string gameSaveDirectory();
bool gamePrepareJourney();
int gameActiveSaveSlot();
enum SaveSlotSelectionResult {
    SAVE_SLOT_SELECTION_CANCELLED,
    SAVE_SLOT_SELECTION_SELECTED,
    SAVE_SLOT_SELECTION_NEW_GAME
};
SaveSlotSelectionResult gameChooseSaveSlotForJourney();
bool gameChooseSaveSlotForNewGame();
std::string gameBeginSaveDirectory();
bool gamePublishSaveDirectory(const std::string &directory, bool dungeon);
bool gameSaveExplorationMap(const std::string &directory);
bool gameSaveEmptyExplorationMap(const std::string &directory);
#ifdef ZU4_IOS
void gameDiscoverWorldPlace(const Coords &coords, Map *destination);
#endif
std::string gameGetRememberedAnswer(const char *prompt);
int gameGetMeditationCycles();
int gameGetAttackRange(int maximum);
bool gameUseQuestItem();
std::string gameGetCharacterName();
std::string gameGetVendorTopic(const std::string &context, const std::vector<std::string> &topics, int maxLength);
void gameObserveVendorDialogue(const std::string &text);
void gameRecordRevealedText(const std::string &text, const std::string &source,
                            const std::string &speaker = "", const std::string &place = "",
                            const std::string &kind = "", const std::string &topic = "");
char gameGetVendorChoice(const std::string &options, const std::string &context, bool continuation = false,
                         const std::vector<std::string> &names = {}, bool allowCancel = true,
                         bool compactPresentation = false, bool controlsOnly = false);
int gameGetAmountInput(int maxDigits, const std::string &context = "");
std::string gameGetMantraInput();
std::string gameGetVirtueInput(const char *prompt, bool includePrinciples = false);
void readyWeapon(int player = -1);

/* checking functions */
void gameCheckHullIntegrity(void);

/* creature functions */
bool creatureRangeAttack(const Coords &coords, Creature *m);
void gameCreatureCleanup(void);
bool gameSpawnCreature(const struct Creature *m);

/* etc */
std::string gameGetInput(int maxlen = 32);
int gameGetPlayer(bool canBeDisabled, bool canBeActivePlayer, const char *prompt = "Choose a party member");
void gameGetPlayerForCommand(bool (*commandFn)(int player), bool canBeDisabled, bool canBeActivePlayer);
void gameDamageParty(int minDamage, int maxDamage);
void gameDamageShip(int minDamage, int maxDamage);
void gameSetActivePlayer(int player);
std::vector<Coords> gameGetDirectionalActionPath(int dirmask, int validDirections, const Coords &origin, int minDistance, int maxDistance, bool (*blockedPredicate)(const Tile *tile), bool includeBlocked);

#endif
