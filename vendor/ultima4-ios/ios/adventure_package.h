#ifndef ZU4_ADVENTURE_PACKAGE_H
#define ZU4_ADVENTURE_PACKAGE_H
#include <map>
#include <string>
#include <vector>

// Portable .u4save contract shared with the browser. No game-data payloads,
// active-slot selection, credentials, or device-specific paths are exported.
namespace AdventurePackage {
using Files = std::map<std::string, std::vector<unsigned char>>;
struct Bundle { Files files; std::string label; double savedAt = 0; };
bool readCheckpoint(const std::string &directory, Bundle &bundle, std::string &error);
bool encode(const Bundle &bundle, std::string &json, std::string &error);
bool decode(const std::string &json, Bundle &bundle, std::string &error);
bool validate(const Files &files, std::string &error);
bool install(const std::string &base, int slot, const Bundle &bundle,
             const std::string &expectedCurrent, std::string &error);
// Native Accounts adapter: serialize checkpoint snapshots without changing saves.
bool cloudSlots(const std::string &base, int activeSlot, std::string &json, std::string &error);
bool installCloudResponse(const std::string &base, int activeSlot, const std::string &json, std::string &error);
void clearCloudLink(int slot);
// Preserve browser-only transcript metadata when native saves rotate generations.
bool copyWebTranscript(const std::string &source, const std::string &destination);
}
#endif
