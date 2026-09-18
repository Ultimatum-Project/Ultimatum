#include "mobile_lifecycle.h"

#include <cassert>

int main() {
    using namespace MobileLifecycle;

    State lifecycle;
    assert(!lifecycle.blocksInput());
    assert(lifecycle.accept(WILL_ENTER_BACKGROUND) == CHECKPOINT);
    assert(lifecycle.blocksInput());
    // The paired DID event must never create a second checkpoint.
    assert(lifecycle.accept(DID_ENTER_BACKGROUND) == NONE);
    assert(lifecycle.blocksInput());
    assert(lifecycle.accept(WILL_ENTER_FOREGROUND) == NONE);
    assert(lifecycle.blocksInput());
    assert(lifecycle.accept(DID_ENTER_FOREGROUND) == RESUME);
    assert(!lifecycle.blocksInput());
    assert(lifecycle.accept(DID_ENTER_FOREGROUND) == NONE);

    // Some platforms can deliver only the DID transition. It is still a
    // complete interruption and receives one checkpoint/resume pair.
    State abbreviated;
    assert(abbreviated.accept(DID_ENTER_BACKGROUND) == CHECKPOINT);
    assert(abbreviated.blocksInput());
    assert(abbreviated.accept(DID_ENTER_FOREGROUND) == RESUME);
    assert(!abbreviated.blocksInput());
    return 0;
}
