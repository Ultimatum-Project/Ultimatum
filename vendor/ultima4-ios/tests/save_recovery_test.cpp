#include "save_recovery.h"

#include <cassert>

int main() {
    using namespace SaveRecovery;

    assert(plan(true, true, true, true) == USE_CURRENT);
    assert(plan(true, false, true, false) == USE_CURRENT);
    assert(plan(false, false, false, false) == USE_LEGACY);

    // Missing, malformed, and semantically corrupt CURRENT all offer the same
    // validated fallback when PREVIOUS can be read safely.
    assert(plan(false, true, false, true) == OFFER_PREVIOUS);
    assert(plan(true, true, false, true) == OFFER_PREVIOUS);

    assert(plan(true, false, false, false) == UNAVAILABLE);
    assert(plan(true, true, false, false) == UNAVAILABLE);
    assert(plan(false, true, false, false) == UNAVAILABLE);
    return 0;
}
