#ifndef ZU4_SAVE_SNAPSHOT_H
#define ZU4_SAVE_SNAPSHOT_H
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>

// Immutable generations: only the small CURRENT pointer is replaced.
// The caller supplies a complete generation before publishing it.
namespace SaveSnapshot {
inline bool syncDirectory(const std::string &path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    bool ok = fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    return ok;
}
inline bool validName(const std::string &name) {
    if (name.empty()) return false;
    for (char c : name)
        if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
            !(c >= '0' && c <= '9') && c != '-' && c != '_') return false;
    return true;
}
inline std::string readPointer(const std::string &root, const std::string &pointerName) {
    if (pointerName != "CURRENT" && pointerName != "PREVIOUS") return "";
    FILE *f = fopen((root + "/" + pointerName).c_str(), "rb");
    if (!f) return "";
    char buffer[128];
    std::size_t count = fread(buffer, 1, sizeof(buffer), f);
    bool ok = !ferror(f) && count > 1 && count < sizeof(buffer);
    if (fclose(f) != 0) ok = false;
    if (!ok || buffer[count - 1] != '\n') return "";
    std::string name(buffer, count - 1);
    if (!validName(name)) return "";
    struct stat info;
    std::string path = root + "/" + name;
    return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode) ? path + "/" : "";
}
inline std::string current(const std::string &root) { return readPointer(root, "CURRENT"); }
inline std::string previous(const std::string &root) { return readPointer(root, "PREVIOUS"); }
inline bool writePointer(const std::string &root, const std::string &pointerName, const std::string &name) {
    if ((pointerName != "CURRENT" && pointerName != "PREVIOUS") || !validName(name)) return false;
    std::string pattern = root + "/pointer-XXXXXX";
    std::vector<char> buffer(pattern.begin(), pattern.end()); buffer.push_back(0);
    int fd = mkstemp(buffer.data());
    if (fd < 0) return false;
    std::string contents = name + "\n";
    bool ok = write(fd, contents.data(), contents.size()) == (ssize_t)contents.size();
    if (ok) ok = fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    if (ok) ok = rename(buffer.data(), (root + "/" + pointerName).c_str()) == 0;
    if (!ok) { unlink(buffer.data()); return false; }
    return syncDirectory(root);
}
inline std::string begin(const std::string &root) {
    if (mkdir(root.c_str(), 0700) != 0) {
        struct stat info;
        if (stat(root.c_str(), &info) != 0 || !S_ISDIR(info.st_mode)) return "";
    }
    std::string pattern = root + "/generation-XXXXXX";
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back(0);
    char *path = mkdtemp(buffer.data());
    return path ? std::string(path) + "/" : "";
}
// Called by the single game save writer only after successful publication.
// Unknown entries and symlinks are never removed.
inline void retainRecent(const std::string &root, const std::string &previous) {
    const std::string selected = current(root);
    if (selected.empty()) return;
    DIR *directory = opendir(root.c_str());
    if (!directory) return;
    struct Candidate { std::string path; time_t modified; std::vector<std::string> files; };
    std::vector<Candidate> candidates;
    const std::vector<std::string> allowed{"party.sav", "monsters.sav", "outmonst.sav", "dngmap.sav", "topics.txt", "journal-notebook.dat", "explored-map.dat", "map-pins.dat", "map-discoveries.dat", "explored-dungeons.dat", "conversations.json"};
    while (dirent *entry = readdir(directory)) {
        std::string name = entry->d_name;
        if (name.compare(0, 11, "generation-") != 0 || !validName(name)) continue;
        std::string path = root + "/" + name;
        struct stat info;
        if (lstat(path.c_str(), &info) != 0 || !S_ISDIR(info.st_mode)) continue;
        DIR *contents = opendir(path.c_str());
        if (!contents) continue;
        Candidate candidate{path + "/", info.st_mtime, {}};
        bool known = true;
        while (dirent *file = readdir(contents)) {
            std::string filename = file->d_name;
            if (filename == "." || filename == "..") continue;
            struct stat fileInfo;
            if (std::find(allowed.begin(), allowed.end(), filename) == allowed.end() ||
                lstat((candidate.path + filename).c_str(), &fileInfo) != 0 || !S_ISREG(fileInfo.st_mode)) {
                known = false; break;
            }
            candidate.files.push_back(filename);
        }
        closedir(contents);
        if (known) candidates.push_back(candidate);
    }
    closedir(directory);
    std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        return a.modified != b.modified ? a.modified > b.modified : a.path > b.path;
    });
    std::vector<std::string> keep{selected};
    const std::string recovery = readPointer(root, "PREVIOUS");
    if (!recovery.empty() && recovery != selected) keep.push_back(recovery);
    if (!previous.empty() && std::find(keep.begin(), keep.end(), previous) == keep.end()) keep.push_back(previous);
    for (const auto &candidate : candidates) {
        if (std::find(keep.begin(), keep.end(), candidate.path) != keep.end()) continue;
        if (keep.size() < 3) { keep.push_back(candidate.path); continue; }
        for (const auto &file : candidate.files) unlink((candidate.path + file).c_str());
        rmdir(candidate.path.c_str());
    }
}

inline bool publish(const std::string &root, const std::string &generation,
                    const std::vector<std::string> &requiredFiles) {
    const std::string prefix = root + "/";
    if (generation.compare(0, prefix.size(), prefix) != 0 || generation.back() != '/') return false;
    std::string name = generation.substr(prefix.size(), generation.size() - prefix.size() - 1);
    if (!validName(name) || requiredFiles.empty()) return false;
    for (const auto &file : requiredFiles) {
        if (file.empty() || file.find('/') != std::string::npos || file == "." || file == "..") return false;
        int fd = open((generation + file).c_str(), O_RDONLY);
        if (fd < 0) return false;
        struct stat info;
        bool ok = fstat(fd, &info) == 0 && S_ISREG(info.st_mode) && info.st_size > 0;
        if (ok) ok = fsync(fd) == 0;
        if (close(fd) != 0) ok = false;
        if (!ok) return false;
    }
    if (!syncDirectory(generation) || !syncDirectory(root)) return false;
    // Preserve only a previously selected generation, never a staging folder.
    // Make that reference durable before switching CURRENT.
    const std::string old = current(root);
    if (!old.empty() && old != generation) {
        std::string oldName = old.substr(root.size() + 1);
        oldName.pop_back();
        if (!writePointer(root, "PREVIOUS", oldName)) return false;
    }
    return writePointer(root, "CURRENT", name);
}
}
#endif
