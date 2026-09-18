#ifndef ZU4_SAVE_STORE_CONTRACT_H
#define ZU4_SAVE_STORE_CONTRACT_H

#include "save_snapshot.h"
#include <string>
#include <vector>

// Native compatibility provider for the platform SaveStore v1 contract.
// Physical generation directories and CURRENT/PREVIOUS pointer files remain
// unchanged; this layer only supplies stable semantic operations and CAS.
namespace SaveStoreContract {
static constexpr const char *version = "1";
static constexpr const char *providerId = "native-generation-save-store-v1";

inline std::string generationId(const std::string &root, const std::string &path) {
    const std::string prefix = root + "/";
    if (path.compare(0, prefix.size(), prefix) != 0 || path.back() != '/') return "";
    return path.substr(prefix.size(), path.size() - prefix.size() - 1);
}

struct Record {
    std::string currentGenerationId;
    std::string previousGenerationId;
};

inline Record inspect(const std::string &root) {
    return {generationId(root, SaveSnapshot::current(root)),
            generationId(root, SaveSnapshot::previous(root))};
}

inline bool publish(const std::string &root, const std::string &generation,
                    const std::vector<std::string> &requiredFiles,
                    const std::string &expectedCurrent = "") {
    const std::string currentPath = SaveSnapshot::current(root);
    const std::string currentId = generationId(root, currentPath);
    // Existing native/cloud callers use the full generation directory as their
    // compare-and-swap token. New platform callers may use the opaque basename.
    if (!expectedCurrent.empty() && expectedCurrent != currentId && expectedCurrent != currentPath) return false;
    return SaveSnapshot::publish(root, generation, requiredFiles);
}

inline bool restore(const std::string &root, const std::string &generation,
                    const std::string &expectedCurrent) {
    const Record record = inspect(root);
    const std::string currentPath = SaveSnapshot::current(root);
    if ((record.currentGenerationId != expectedCurrent && currentPath != expectedCurrent) ||
        record.previousGenerationId != generation || !SaveSnapshot::validName(generation)) return false;
    return SaveSnapshot::writePointer(root, "CURRENT", generation);
}
}

#endif
