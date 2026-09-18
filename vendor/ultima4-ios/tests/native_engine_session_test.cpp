#include "native_engine_session.h"

#include <cassert>

int main() {
    using namespace NativeEngineSession;

    Session session;
    assert(session.contractVersion() == 1);
    assert(session.state() == State::RUNNING);
    assert(!session.blocksInput());

    Transition background = session.handle(LifecycleEvent::WILL_ENTER_BACKGROUND);
    assert(background.handled);
    assert(includes(background.actions, PAUSE));
    assert(includes(background.actions, CHECKPOINT));
    assert(background.state == State::QUIESCED);
    assert(session.blocksInput());

    // Paired SDL lifecycle events must not create duplicate checkpoints.
    assert(session.handle(LifecycleEvent::DID_ENTER_BACKGROUND).actions == NONE);
    assert(session.handle(LifecycleEvent::WILL_ENTER_FOREGROUND).actions == NONE);
    Transition foreground = session.handle(LifecycleEvent::DID_ENTER_FOREGROUND);
    assert(foreground.actions == RESUME);
    assert(foreground.state == State::RUNNING);
    assert(!session.blocksInput());
    assert(session.handle(LifecycleEvent::DID_ENTER_FOREGROUND).actions == NONE);

    // Hosts that deliver only DID events still get one checkpoint/resume pair.
    Session abbreviated;
    assert(includes(abbreviated.handle(LifecycleEvent::DID_ENTER_BACKGROUND).actions, CHECKPOINT));
    assert(abbreviated.handle(LifecycleEvent::DID_ENTER_FOREGROUND).actions == RESUME);

    // Explicit session operations expose the same compatibility semantics.
    Session explicitSession;
    assert(explicitSession.pause().actions == PAUSE);
    assert(explicitSession.state() == State::PAUSED);
    assert(explicitSession.requestCheckpoint().actions == CHECKPOINT);
    assert(explicitSession.resume().actions == RESUME);
    Transition quiesced = explicitSession.quiesce();
    assert(includes(quiesced.actions, PAUSE));
    assert(includes(quiesced.actions, CHECKPOINT));
    assert(explicitSession.quiesce().actions == NONE);
    // Quiesce already requested durability, so shutdown must not duplicate it.
    assert(explicitSession.shutdown().actions == SHUTDOWN);
    assert(explicitSession.state() == State::STOPPED);
    assert(explicitSession.blocksInput());
    assert(explicitSession.resume().actions == NONE);
    assert(explicitSession.requestCheckpoint().actions == NONE);

    Session orderlyQuit;
    Transition stopped = orderlyQuit.shutdown();
    assert(includes(stopped.actions, PAUSE));
    assert(includes(stopped.actions, CHECKPOINT));
    assert(includes(stopped.actions, SHUTDOWN));
    assert(orderlyQuit.shutdown().actions == NONE);
    return 0;
}
