#include "mobile_combat.h"
#include <cassert>

int main() {
    MobileCombat::Identity enemy, other, clone(enemy);
    assert(enemy.value != other.value && enemy.value != clone.value);
    uint64_t original = other.value;
    other = enemy;
    assert(other.value == original);
    int sword = 0, bow = 0;
    MobileCombat::LastAttack attacks[8];
    assert(!attacks[0].matches(enemy.value, &sword));
    attacks[0] = {enemy.value, &sword};
    assert(attacks[0].matches(enemy.value, &sword));
    assert(!attacks[0].matches(other.value, &sword));
    assert(!attacks[0].matches(enemy.value, &bow));
    assert(!attacks[1].matches(enemy.value, &sword));
    attacks[0].clear();
    assert(!attacks[0].matches(enemy.value, &sword));
    assert(MobileCombat::flashMilliseconds(1, false) == 33);
    assert(MobileCombat::flashMilliseconds(3, false) == 99);
    assert(MobileCombat::flashMilliseconds(1, true) == 33);
    assert(MobileCombat::flashMilliseconds(3, true) == 49);
    assert(MobileCombat::flashMilliseconds(0, true) == 0);
    assert(MobileCombat::roundPauseMilliseconds(false) == 50);
    assert(MobileCombat::roundPauseMilliseconds(true) == 25);
}
