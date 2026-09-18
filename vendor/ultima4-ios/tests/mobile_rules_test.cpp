#include "mobile_rules.h"
#include "mobile_context_action.h"
#include "mobile_adjacent_action.h"
#include "mobile_pathfinding.h"
#include "mobile_tap_action.h"
#include <cassert>
int main() {
    short stock[] = {8, 3, 0, 12, 0, 0, 0, 0};
    assert(MobileRules::mixCapacity(3, stock, 8, 0) == 3);
    assert(MobileRules::mixCapacity(5, stock, 8, 0) == 0);
    assert(MobileRules::mixCapacity(3, stock, 8, 98) == 1);
    assert(MobileRules::mixCapacity(3, stock, 8, 99) == 0);
    assert(MobileRules::mixCapacity(3, stock, 8, 100) == 0);
    assert(MobileRules::mixCapacity(0, stock, 8, 0) == 0);
    stock[7] = -1;
    assert(MobileRules::mixCapacity(128, stock, 8, 0) == 0);
    assert(stock[0] == 8 && stock[1] == 3); // browsing never reserves resources

    MobileContext::State context = {};
    context.transport = MobileContext::FOOT;
    assert(MobileContext::resolve(context) == MobileContext::NONE);
    assert(MobileContext::command(MobileContext::resolve(context)) == 0);
    context.boardableHere = true;
    assert(MobileContext::resolve(context) == MobileContext::BOARD);
    context.enterPortal = true;
    assert(MobileContext::resolve(context) == MobileContext::ENTER);
    context.chestHere = true;
    assert(MobileContext::resolve(context) == MobileContext::OPEN_CHEST);
    assert(MobileContext::command(MobileContext::resolve(context)) == 'g');
    context = {};
    context.transport = MobileContext::BALLOON;
    assert(MobileContext::resolve(context) == MobileContext::ASCEND);
    context.flying = true;
    assert(MobileContext::resolve(context) == MobileContext::LAND);
    context = {};
    context.transport = MobileContext::FOOT;
    context.dungeon = true;
    context.ladderDown = true;
    assert(MobileContext::resolve(context) == MobileContext::DESCEND);
    context.ladderUp = true;
    assert(MobileContext::resolve(context) == MobileContext::CLIMB);

    MobileAdjacent::State adjacent = {};
    adjacent.enabled = true;
    adjacent.ordinaryExploration = true;
    adjacent.deliberateInput = true;
    adjacent.footOrHorse = true;
    adjacent.target = MobileAdjacent::CONVERSABLE_PERSON;
    assert(MobileAdjacent::resolve(adjacent) == MobileAdjacent::TALK);
    adjacent.target = MobileAdjacent::UNLOCKED_DOOR;
    assert(MobileAdjacent::resolve(adjacent) == MobileAdjacent::OPEN);
    adjacent.target = MobileAdjacent::LOCKED_DOOR;
    assert(MobileAdjacent::resolve(adjacent) == MobileAdjacent::LOCKED_NOTICE);
    adjacent.target = MobileAdjacent::HOSTILE_PERSON;
    assert(MobileAdjacent::resolve(adjacent) == MobileAdjacent::NONE);
    adjacent.target = MobileAdjacent::CONVERSABLE_PERSON;
    adjacent.deliberateInput = false; // held-direction repeat
    assert(MobileAdjacent::resolve(adjacent) == MobileAdjacent::NONE);
    adjacent.deliberateInput = true;
    adjacent.ordinaryExploration = false;
    assert(MobileAdjacent::resolve(adjacent) == MobileAdjacent::NONE);
    adjacent.ordinaryExploration = true;
    adjacent.collisionOverride = true;
    assert(MobileAdjacent::resolve(adjacent) == MobileAdjacent::NONE);
    adjacent.collisionOverride = false;
    adjacent.enabled = false;
    assert(MobileAdjacent::resolve(adjacent) == MobileAdjacent::NONE);

    const int side = 5;
    std::vector<int> moves(side * side,
        MASK_DIR_WEST | MASK_DIR_NORTH | MASK_DIR_EAST | MASK_DIR_SOUTH);
    std::vector<unsigned char> hazards(side * side, 0);
    std::vector<Direction> path = MobilePathfinding::shortestPath(
        side, side, {2, 2}, {4, 2}, moves, hazards, 6);
    assert(path.size() == 2 && path[0] == DIR_EAST && path[1] == DIR_EAST);

    hazards[2 * side + 3] = 1;
    path = MobilePathfinding::shortestPath(
        side, side, {2, 2}, {4, 2}, moves, hazards, 6);
    assert(path.size() == 4);
    for (Direction direction : path) assert(direction != DIR_NONE);

    hazards[2 * side + 4] = 1;
    assert(MobilePathfinding::shortestPath(
        side, side, {2, 2}, {4, 2}, moves, hazards, 6).empty());
    hazards.assign(side * side, 0);
    assert(MobilePathfinding::shortestPath(
        side, side, {2, 2}, {4, 2}, moves, hazards, 1).empty());

    moves.assign(side * side, 0);
    assert(MobilePathfinding::shortestPath(
        side, side, {2, 2}, {2, 3}, moves, hazards, 6).empty());

    moves.assign(side * side,
        MASK_DIR_WEST | MASK_DIR_NORTH | MASK_DIR_EAST | MASK_DIR_SOUTH);
    MobilePathfinding::Route route = MobilePathfinding::shortestPathToAny(
        side, side, {2, 2}, {{4, 2}, {2, 3}}, moves, hazards, 6);
    assert(route.found && route.target.x == 2 && route.target.y == 3);
    assert(route.steps.size() == 1 && route.steps[0] == DIR_SOUTH);
    route = MobilePathfinding::shortestPathToAny(
        side, side, {2, 2}, {{2, 2}, {4, 2}}, moves, hazards, 6);
    assert(route.found && route.steps.empty());

    std::vector<MobilePathfinding::Point> approaches = MobileTap::approachPoints(
        MobileTap::PERSON, {2, 2}, MASK_DIR(DIR_NORTH));
    assert(approaches.size() == 5);
    assert(approaches[1].x == 2 && approaches[1].y == 1);
    assert(approaches[2].x == 2 && approaches[2].y == 0);
    assert(MobileTap::approachPoints(MobileTap::UNLOCKED_DOOR, {2, 2}).size() == 4);
    approaches = MobileTap::approachPoints(MobileTap::CHEST, {2, 2});
    assert(approaches.size() == 1 && approaches[0].x == 2 && approaches[0].y == 2);
}
