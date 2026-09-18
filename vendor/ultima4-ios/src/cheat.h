/*
 * $Id: cheat.h 2515 2005-09-14 04:09:03Z andrewtaylor $
 */

#ifndef CHEAT_H
#define CHEAT_H

#include <string>

#include "controller.h"
#include "direction.h"

struct GameController;

struct CheatMenuController : public WaitableController<void *> {
public:
    CheatMenuController(GameController *game);
    bool keyPressed(int key);

private:
    GameController *game;
};

// Semantic entry points shared by the legacy keyboard cheat controller and
// the native iOS Test Tools screen.
bool cheatSummonCreature(const std::string &name);
bool cheatCreateTransport(char transport, Direction direction);

/**
 * This struct controls the wind option from the cheat menu.  It
 * handles setting the wind direction as well as locking/unlocking.
 * The value field of WaitableController isn't used.
 */
struct WindCmdController : public WaitableController<void *> {
public:
    bool keyPressed(int key);
};

#endif /* CHEAT_H */
