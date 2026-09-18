#ifndef ZU4_MOBILE_LIFECYCLE_H
#define ZU4_MOBILE_LIFECYCLE_H

// SDL sends paired WILL/DID lifecycle notifications. Keep the policy separate
// from SDL so duplicate delivery, input blocking, and resume behavior can be
// regression-tested without an iOS runtime.
namespace MobileLifecycle {

enum Event {
    WILL_ENTER_BACKGROUND,
    DID_ENTER_BACKGROUND,
    WILL_ENTER_FOREGROUND,
    DID_ENTER_FOREGROUND
};

enum Action {
    NONE = 0,
    CHECKPOINT = 1,
    RESUME = 2
};

class State {
public:
    State() : backgrounded_(false) {}

    Action accept(Event event) {
        switch (event) {
        case WILL_ENTER_BACKGROUND:
        case DID_ENTER_BACKGROUND:
            if (backgrounded_) return NONE;
            backgrounded_ = true;
            return CHECKPOINT;
        case WILL_ENTER_FOREGROUND:
            return NONE;
        case DID_ENTER_FOREGROUND:
            if (!backgrounded_) return NONE;
            backgrounded_ = false;
            return RESUME;
        }
        return NONE;
    }

    bool blocksInput() const { return backgrounded_; }

private:
    bool backgrounded_;
};

} // namespace MobileLifecycle

#endif
