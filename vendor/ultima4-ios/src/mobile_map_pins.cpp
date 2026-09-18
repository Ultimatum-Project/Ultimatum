#include "mobile_map_pins.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <iomanip>

namespace {
bool validLabel(const std::string &label) {
    if (label.empty() || label.size() > MobileMapPins::MAX_LABEL_BYTES) return false;
    for (unsigned char ch : label)
        if (ch < 0x20 || ch == 0x7f) return false;
    return true;
}

bool validPosition(int x, int y, int width, int height) {
    return width > 0 && height > 0 && x >= 0 && y >= 0 && x < width && y < height;
}
}

bool MobileMapPins::set(int x, int y, const std::string &label, int width, int height) {
    if (!validPosition(x, y, width, height) || !validLabel(label)) return false;
    auto existing = std::find_if(pins.begin(), pins.end(), [&](const Pin &pin) {
        return pin.x == x && pin.y == y;
    });
    if (existing != pins.end()) {
        existing->label = label;
        return true;
    }
    if (pins.size() >= MAX_PINS) return false;
    pins.push_back({x, y, label});
    return true;
}

bool MobileMapPins::remove(int x, int y) {
    auto existing = std::find_if(pins.begin(), pins.end(), [&](const Pin &pin) {
        return pin.x == x && pin.y == y;
    });
    if (existing == pins.end()) return false;
    pins.erase(existing);
    return true;
}

bool MobileMapPins::save(const std::string &path) const {
    const std::string temporary = path + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) return false;
    output << "U4MAP-PINS 1\n";
    for (const Pin &pin : pins)
        output << pin.x << ' ' << pin.y << ' ' << std::quoted(pin.label) << '\n';
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

bool MobileMapPins::load(const std::string &path, int width, int height) {
    errno = 0;
    std::ifstream input(path);
    if (!input) {
        if (errno == ENOENT) {
            pins.clear();
            return true;
        }
        return false;
    }
    std::string header;
    if (!std::getline(input, header) || header != "U4MAP-PINS 1") return false;
    std::vector<Pin> loaded;
    while (true) {
        input >> std::ws;
        if (input.eof()) break;
        Pin pin = {};
        if (!(input >> pin.x >> pin.y >> std::quoted(pin.label)) ||
            !validPosition(pin.x, pin.y, width, height) || !validLabel(pin.label) ||
            loaded.size() >= MAX_PINS) return false;
        if (std::any_of(loaded.begin(), loaded.end(), [&](const Pin &other) {
                return other.x == pin.x && other.y == pin.y;
            })) return false;
        loaded.push_back(pin);
    }
    if (input.bad()) return false;
    pins.swap(loaded);
    return true;
}
