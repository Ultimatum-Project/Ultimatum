#include <emscripten/emscripten.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <set>
#include <string>
#include <sys/stat.h>
#include "miniz.h"

// Unpack only into a disposable staging directory. Never activate game files
// or touch saves until JavaScript has verified the complete compatibility set.
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_extract_game_zip(const unsigned char *data, int size) {
    if (!data || size < 1 || size > 128 * 1024 * 1024) return -1;
    mz_zip_archive archive = {};
    if (!mz_zip_reader_init_mem(&archive, data, size, 0)) return -2;
    const mz_uint count = mz_zip_reader_get_num_files(&archive);
    int result = 0;
    size_t total = 0;
    std::set<std::string> names;
    mkdir("/game-data-check", 0777);
    if (count > 2048) result = -3;
    for (mz_uint i = 0; i < count && result >= 0; ++i) {
        mz_zip_archive_file_stat stat = {};
        if (!mz_zip_reader_file_stat(&archive, i, &stat)) { result = -2; break; }
        if (stat.m_is_directory) continue;
        std::string path = stat.m_filename;
        std::replace(path.begin(), path.end(), '\\', '/');
        bool unsafe = path.empty() || path.size() > 255 || path[0] == '/' || path.find(':') != std::string::npos;
        std::string canonical;
        size_t start = 0;
        while (start < path.size()) {
            const size_t end = path.find('/', start);
            const std::string part = path.substr(start, end - start);
            if (part == "..") unsafe = true;
            if (!part.empty() && part != ".") {if (!canonical.empty()) canonical += '/'; canonical += part;}
            if (end == std::string::npos) break;
            start = end + 1;
        }
        for (unsigned char ch : path) if (ch < 32 || ch == 127) unsafe = true;
        if (unsafe) { result = -4; break; }
        std::transform(canonical.begin(), canonical.end(), canonical.begin(), [](unsigned char ch) { return std::toupper(ch); });
        std::string name = canonical.substr(canonical.find_last_of('/') + 1);
        const size_t dot = name.find_last_of('.');
        if (dot == std::string::npos) continue;
        const std::string ext = name.substr(dot);
        if (ext != ".MAP" && ext != ".ULT" && ext != ".TLK" && ext != ".DNG" && ext != ".CON" && ext != ".EGA" && name != "AVATAR.EXE" && name != "TITLE.EXE") continue;
        if (name.size() > 12 || !names.insert(canonical).second) { result = -5; break; }
        if (!stat.m_is_supported || stat.m_is_encrypted || stat.m_uncomp_size == 0 || stat.m_uncomp_size > 2 * 1024 * 1024) { result = -6; break; }
        total += stat.m_uncomp_size;
        if (total > 32 * 1024 * 1024) { result = -3; break; }
        size_t extracted = 0;
        void *bytes = mz_zip_reader_extract_to_heap(&archive, i, &extracted, 0);
        if (!bytes || extracted != stat.m_uncomp_size) { if (bytes) mz_free(bytes); result = -7; break; }
        // Preserve installation boundaries, including nested upgrade folders.
        // Only JavaScript's complete-set selection may flatten accepted files.
        for (size_t slash = canonical.find('/'); slash != std::string::npos; slash = canonical.find('/',slash+1))
            mkdir(("/game-data-check/" + canonical.substr(0,slash)).c_str(),0777);
        FILE *file = fopen(("/game-data-check/" + canonical).c_str(), "wb");
        bool written = false;
        if (file) { written = fwrite(bytes, 1, extracted, file) == extracted; written = fclose(file) == 0 && written; }
        mz_free(bytes);
        if (!written) { result = -8; break; }
        ++result;
    }
    mz_zip_reader_end(&archive);
    return result;
}
