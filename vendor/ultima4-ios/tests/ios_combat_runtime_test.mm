// Opt-in, simulator-only real-engine tests. Run in org.ultimatumproject.tests.combat;
// no production adventures or physical-device builds include this fixture.
#import <Foundation/Foundation.h>
#include <cstdio>
#include <cstdlib>
#include "combat.h"
#include "context.h"
#include "game.h"
#include "mapmgr.h"
#include "settings.h"
#include "stats.h"
#include "tileset.h"
#include "annotation.h"
#include "topic_panel.h"
#include "zu4_ios_ui.h"
#include "video.h"

static void check(bool valid, const char *message) {
    if (!valid) { fprintf(stderr, "FAIL %s\n", message); fflush(stderr); abort(); }
    fprintf(stderr, "PASS %s\n", message); fflush(stderr);
}

void zu4_run_combat_runtime_tests() {
    check([[NSBundle.mainBundle bundleIdentifier] isEqualToString:@"org.ultimatumproject.tests.combat"],
          "isolated simulator bundle");
    while (c->location->prev) game->exitToParentMap();
    game->paused = true; // deterministic tests: freeze unrelated real-time wind/moon ticks
    c->saveGame->members = 2;
    for (int i = 0; i < 2; ++i) {
        c->saveGame->players[i].hp = c->saveGame->players[i].hpMax = 800;
        c->saveGame->players[i].status = STAT_GOOD;
        c->saveGame->players[i].weapon = WEAP_SWORD;
        c->saveGame->players[i].klass = CLASS_FIGHTER;
        c->saveGame->players[i].dex = 50;
    }
    delete c->party;
    c->party = new Party(c->saveGame);
    c->party->addObserver(game);
    c->party->addObserver(c->stats);
    c->party->setTransport(Tileset::findTileByName("avatar")->getId());
    CombatController *combat = new CombatController(MAP_BRICK_CON);
    combat->init(nullptr);
    combat->setWinOrLose(false);
    combat->begin();
    CombatMap *map = combat->getMap();
    auto *actor = combat->getCurrentPlayer();
    actor->setCoords({5,7,0});
    (*combat->getParty())[1]->setCoords({6,7,0});
    Creature *enemy = map->addCreature(creatureMgr->getByName("rat"), {5,6,0});
    enemy->setHp(10000);
    check(!zu4_mobile_combat_repeat_target(nullptr, 0), "repeat disabled before first attack");
    int food = c->saveGame->food, moves = c->saveGame->moves;
    check(combat->mobileSelectTarget(enemy->getCoords()), "select real attack target");
    zu4_mobile_talk(); // production Attack dispatcher and original finishTurn
    check(combat->getFocus() == 1 && c->saveGame->food == food - 1,
          "ordinary attack advances exactly one fighter and food cost");
    check(!zu4_mobile_combat_repeat_target(nullptr, 0), "repeat memory is per fighter");
    combat->setActivePlayer(0);
    food = c->saveGame->food; moves = c->saveGame->moves;
    zu4_mobile_perform_action(ZU4_MOBILE_ACTION_COMBAT_REPEAT_ATTACK, 0);
    MobileCombatTarget selected;
    check(combat->mobileSelectedTarget(&selected) && selected.creature == enemy,
          "repeat prepares same enemy with visible target preview");
    check(c->saveGame->food == food && c->saveGame->moves == moves && combat->getFocus() == 0,
          "repeat preparation uses no turn or resources");
    zu4_mobile_perform_action(ZU4_MOBILE_ACTION_COMBAT_CLEAR_TARGET, 0);
    check(!combat->mobileSelectedTarget(nullptr) && c->saveGame->food == food,
          "Clear cancels repeat without turn cost");
    zu4_mobile_perform_action(ZU4_MOBILE_ACTION_COMBAT_REPEAT_ATTACK, 0);
    zu4_mobile_talk();
    check(combat->getFocus() == 1 && c->saveGame->food == food - 1,
          "Repeat then Attack confirms exactly one ordinary fighter turn");
    combat->setActivePlayer(0);
    food = c->saveGame->food;
    // Exercise native direction cancellation through the panel's production API.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
        check(zu4_topic_panel_direction_active(), "native direction prompt opened");
        check(zu4_topic_panel_choose_direction("cancel"), "native direction cancellation submitted");
    });
    zu4_mobile_talk();
    check(combat->getFocus() == 0 && c->saveGame->food == food,
          "cancelled attack returns to same fighter");
    enemy->setCoords({5,5,0});
    check(!combat->mobileLastAttackTarget(nullptr), "out-of-range target rejects repeat");
    enemy->setCoords({5,6,0});
    check(combat->mobileLastAttackTarget(nullptr), "same reachable enemy can repeat");
    actor->putToSleep();
    check(!combat->mobileLastAttackTarget(nullptr), "disabled fighter cannot repeat");
    actor->wakeUp();
    c->saveGame->players[0].weapon = WEAP_BOW;
    check(!combat->mobileLastAttackTarget(nullptr), "changed weapon rejects old attack");
    check(combat->mobileSelectTarget(enemy->getCoords()), "new ranged target selected");
    check(combat->attack(), "original ranged attack runs");
    enemy->setCoords({5,4,0});
    map->annotations->add({5,5,0}, MapTile(Tileset::findTileByName("brick_wall")->getId()), false, true);
    check(!combat->mobileLastAttackTarget(nullptr), "blocked path rejects repeat");
    map->annotations->clear();
    check(combat->mobilePrepareLastAttack(), "moving same enemy revalidated at current coordinates");
    enemy->setCoords({4,4,0});
    check(!combat->attack() && combat->getFocus() == 0,
          "invalidated prepared repeat never falls back to blind attack");
    enemy->setCoords({5,6,0});
    check(combat->mobilePrepareLastAttack(), "repeat can be prepared again");
    c->saveGame->players[0].weapon = WEAP_SWORD;
    check(!combat->attack(), "weapon changed after preview rejects confirmation");
    c->saveGame->players[0].weapon = WEAP_BOW;
    uint64_t oldIdentity = enemy->mobileCombatIdentity.value;
    map->removeObject(enemy);
    enemy = map->addCreature(creatureMgr->getByName("rat"), {5,6,0});
    enemy->setHp(10000);
    check(oldIdentity != enemy->mobileCombatIdentity.value && !combat->mobileLastAttackTarget(nullptr),
          "dead enemy replacement on same tile is not repeated");
    // Original costly/absolute-range and returning-weapon attack paths remain intact.
    for (WeaponType weapon : {WEAP_OIL, WEAP_HALBERD, WEAP_MAGICAXE}) {
        combat->mobileClearTarget();
        c->saveGame->players[0].weapon = weapon;
        c->saveGame->weapons[weapon] = 4;
        enemy->setCoords({5,5,0}); enemy->setHp(10000);
        check(combat->mobileSelectTarget(enemy->getCoords()), "special weapon target valid");
        check(combat->attack(), "special weapon uses original attack path");
        check(c->saveGame->weapons[weapon] == (weapon == WEAP_OIL ? 3 : 4),
              "original consumable and returning weapon costs preserved");
        map->annotations->clear();
    }
    c->saveGame->players[0].weapon = WEAP_SWORD;
    enemy->setCoords({5,6,0}); enemy->setHp(10000);
    int damage[2];
    double elapsed[2];
    for (int fast = 0; fast < 2; ++fast) {
        settings.fastCombatPresentation = fast != 0;
        enemy->setHp(10000);
        combat->mobileClearTarget(); combat->mobileSelectTarget(enemy->getCoords());
        // random.c selects rand() or random() by the platform's BSD macro.
        srand(9182); srandom(9182);
        double start = CFAbsoluteTimeGetCurrent();
        check(combat->attack(), "paced attack executes original engine");
        elapsed[fast] = CFAbsoluteTimeGetCurrent() - start;
        damage[fast] = 10000 - enemy->getHp();
    }
    fprintf(stderr, "DAMAGE Standard %d Fast %d\n", damage[0], damage[1]);
    check(damage[0] == damage[1], "Standard/Fast seeded attacks have identical damage");
    fprintf(stderr, "TIMING Standard %.3fs Fast %.3fs damage %d\n", elapsed[0], elapsed[1], damage[0]);
    combat->mobileClearTarget();
    combat->end(false);
    check(eventHandler->getController() == game, "battle exit returns to main controls");
    combat = new CombatController(MAP_BRICK_CON);
    combat->init(nullptr); combat->setWinOrLose(false); combat->begin();
    actor = combat->getCurrentPlayer(); actor->setCoords({5,7,0});
    enemy = combat->getMap()->addCreature(creatureMgr->getByName("rat"), {5,6,0}); enemy->setHp(10000);
    check(!combat->mobileLastAttackTarget(nullptr), "new battle clears repeat history");
    combat->mobileSelectTarget(enemy->getCoords()); combat->attack();
    combat->mobilePrepareLastAttack();
    gameUpdateScreen();
    zu4_ogl_swap();
    fprintf(stderr, "PASS combat runtime suite complete; holding repeat preview for screenshot\n");
    fflush(stderr);
}
