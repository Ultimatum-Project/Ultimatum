#ifndef ZU4_MOBILE_RULES_H
#define ZU4_MOBILE_RULES_H
#include <algorithm>
#include <cstddef>
namespace MobileRules {
// Compute availability without reserving or mutating any ingredient.
inline int mixCapacity(unsigned components, const short *reagents, std::size_t count, int mixtures) {
    if (!components || mixtures < 0 || mixtures >= 99) return 0;
    int capacity = 99 - mixtures;
    for (std::size_t i = 0; i < count; ++i)
        if (components & (1u << i)) capacity = std::max(0, std::min(capacity, (int)reagents[i]));
    return capacity;
}
}
#endif
