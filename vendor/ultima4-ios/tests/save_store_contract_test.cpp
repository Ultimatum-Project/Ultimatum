#include "save_store_contract.h"
#include <cassert>
#include <fstream>
#include <string>
#include <unistd.h>

int main() {
    char directory[] = "/private/tmp/u4-save-store-contract-XXXXXX";
    assert(mkdtemp(directory));
    const std::string root = directory;
    const std::vector<std::string> required{"party.sav"};
    auto first = SaveSnapshot::begin(root);
    std::ofstream(first + "party.sav") << "first";
    assert(SaveStoreContract::publish(root, first, required));
    auto firstRecord = SaveStoreContract::inspect(root);
    assert(firstRecord.currentGenerationId == SaveStoreContract::generationId(root, first));

    auto second = SaveSnapshot::begin(root);
    std::ofstream(second + "party.sav") << "second";
    assert(!SaveStoreContract::publish(root, second, required, "stale-generation"));
    assert(SaveStoreContract::publish(root, second, required, first));
    auto secondRecord = SaveStoreContract::inspect(root);
    assert(secondRecord.previousGenerationId == firstRecord.currentGenerationId);
    assert(!SaveStoreContract::restore(root, firstRecord.currentGenerationId, "stale-generation"));
    assert(SaveStoreContract::restore(root, firstRecord.currentGenerationId, secondRecord.currentGenerationId));
    assert(SaveStoreContract::inspect(root).currentGenerationId == firstRecord.currentGenerationId);

    for (const auto &generation : {first, second}) {
        unlink((generation + "party.sav").c_str());
        rmdir(generation.c_str());
    }
    unlink((root + "/CURRENT").c_str());
    unlink((root + "/PREVIOUS").c_str());
    rmdir(root.c_str());
}
