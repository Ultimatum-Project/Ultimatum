#include "mobile_map_discoveries.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <iomanip>

namespace {
bool validPosition(int x, int y, int width, int height) {
    return width > 0 && height > 0 && x >= 0 && y >= 0 && x < width && y < height;
}

bool validCategory(MobileMapDiscoveries::Category category) {
    return category >= MobileMapDiscoveries::TOWN &&
        category <= MobileMapDiscoveries::DUNGEON;
}

bool validName(const std::string &name) {
    if (name.empty() || name.size() > MobileMapDiscoveries::MAX_NAME_BYTES) return false;
    for (unsigned char ch : name)
        if (ch < 0x20 || ch == 0x7f) return false;
    return true;
}
}

bool MobileMapDiscoveries::discover(int x, int y, Category category,
                                    const std::string &name, int width, int height) {
    if (!validPosition(x, y, width, height) || !validCategory(category) || !validName(name))
        return false;
    auto existing = std::find_if(places.begin(), places.end(), [&](const Place &place) {
        return place.x == x && place.y == y;
    });
    if (existing != places.end()) {
        existing->category = category;
        existing->name = name;
        return true;
    }
    if (places.size() >= MAX_PLACES) return false;
    places.push_back({x, y, category, name});
    return true;
}

bool MobileMapDiscoveries::save(const std::string &path) const {
    const std::string temporary = path + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) return false;
    output << "U4MAP-DISCOVERIES 1\n";
    for (const Place &place : places)
        output << place.x << ' ' << place.y << ' ' << (int)place.category << ' '
               << std::quoted(place.name) << '\n';
    output.flush();
    if (!output) {
        output.close();
        std::remove(temporary.c_str());
        return false;
    }
    output.close();
    if (!output || std::rename(temporary.c_str(), path.c_str()) != 0) {
        std::remove(temporary.c_str());
        return false;
    }
    return true;
}

bool MobileMapDiscoveries::load(const std::string &path, int width, int height) {
    errno = 0;
    std::ifstream input(path);
    if (!input) {
        if (errno == ENOENT) {
            places.clear();
            return true;
        }
        return false;
    }
    std::string header;
    if (!std::getline(input, header) || header != "U4MAP-DISCOVERIES 1") return false;
    std::vector<Place> loaded;
    while (true) {
        input >> std::ws;
        if (input.eof()) break;
        Place place = {};
        int category = 0;
        if (!(input >> place.x >> place.y >> category >> std::quoted(place.name))) return false;
        place.category = (Category)category;
        if (!validPosition(place.x, place.y, width, height) ||
            !validCategory(place.category) || !validName(place.name) ||
            loaded.size() >= MAX_PLACES) return false;
        if (std::any_of(loaded.begin(), loaded.end(), [&](const Place &other) {
                return other.x == place.x && other.y == place.y;
            })) return false;
        loaded.push_back(place);
    }
    if (input.bad()) return false;
    places.swap(loaded);
    return true;
}
