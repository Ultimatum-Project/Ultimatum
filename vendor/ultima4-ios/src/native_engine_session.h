#ifndef ZU4_NATIVE_ENGINE_SESSION_H
#define ZU4_NATIVE_ENGINE_SESSION_H

// Compatibility implementation of the platform EngineSession lifecycle for
// native hosts. It contains policy only: the SDL/iOS adapter executes the
// returned actions on the engine thread, keeping UIKit callbacks from
// re-entering the running engine.
namespace NativeEngineSession {

static const int CONTRACT_VERSION = 1;

enum class State {
    RUNNING,
    PAUSED,
    QUIESCED,
    STOPPED
};

enum class LifecycleEvent {
    WILL_ENTER_BACKGROUND,
    DID_ENTER_BACKGROUND,
    WILL_ENTER_FOREGROUND,
    DID_ENTER_FOREGROUND
};

enum Action {
    NONE = 0,
    PAUSE = 1 << 0,
    CHECKPOINT = 1 << 1,
    RESUME = 1 << 2,
    SHUTDOWN = 1 << 3
};

inline Action operator|(Action left, Action right) {
    return static_cast<Action>(static_cast<int>(left) | static_cast<int>(right));
}

inline bool includes(Action actions, Action expected) {
    return (static_cast<int>(actions) & static_cast<int>(expected)) != 0;
}

struct Transition {
    bool handled;
    Action actions;
    State state;
};

class Session {
public:
    Session() : state_(State::RUNNING), interruptionActive_(false) {}

    int contractVersion() const { return CONTRACT_VERSION; }
    State state() const { return state_; }
    bool blocksInput() const { return state_ != State::RUNNING; }

    Transition handle(LifecycleEvent event) {
        if (state_ == State::STOPPED) return transition(true, NONE);
        switch (event) {
        case LifecycleEvent::WILL_ENTER_BACKGROUND:
        case LifecycleEvent::DID_ENTER_BACKGROUND:
            if (interruptionActive_) return transition(true, NONE);
            interruptionActive_ = true;
            state_ = State::QUIESCED;
            return transition(true, PAUSE | CHECKPOINT);
        case LifecycleEvent::WILL_ENTER_FOREGROUND:
            return transition(true, NONE);
        case LifecycleEvent::DID_ENTER_FOREGROUND:
            if (!interruptionActive_) return transition(true, NONE);
            interruptionActive_ = false;
            state_ = State::RUNNING;
            return transition(true, RESUME);
        }
        return transition(false, NONE);
    }

    Transition pause() {
        if (state_ != State::RUNNING) return transition(true, NONE);
        state_ = State::PAUSED;
        return transition(true, PAUSE);
    }

    Transition resume() {
        if (state_ == State::STOPPED || interruptionActive_ || state_ == State::RUNNING)
            return transition(true, NONE);
        state_ = State::RUNNING;
        return transition(true, RESUME);
    }

    Transition requestCheckpoint() {
        if (state_ == State::STOPPED) return transition(true, NONE);
        return transition(true, CHECKPOINT);
    }

    Transition quiesce() {
        if (state_ == State::STOPPED || state_ == State::QUIESCED)
            return transition(true, NONE);
        Action actions = CHECKPOINT;
        if (state_ == State::RUNNING) actions = PAUSE | CHECKPOINT;
        state_ = State::QUIESCED;
        return transition(true, actions);
    }

    Transition shutdown() {
        if (state_ == State::STOPPED) return transition(true, NONE);
        Action actions = SHUTDOWN;
        // An orderly quit from active play receives one final safe checkpoint.
        // A backgrounded/quiesced session already requested that checkpoint.
        if (state_ == State::RUNNING) actions = PAUSE | CHECKPOINT | SHUTDOWN;
        state_ = State::STOPPED;
        interruptionActive_ = false;
        return transition(true, actions);
    }

private:
    Transition transition(bool handled, Action actions) const {
        return Transition{handled, actions, state_};
    }

    State state_;
    bool interruptionActive_;
};

} // namespace NativeEngineSession

#endif
