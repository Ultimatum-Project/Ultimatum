#ifndef ZU4_MOBILE_MAP_PINS_H
#define ZU4_MOBILE_MAP_PINS_H

#include <string>
#include <vector>

class MobileMapPins {
public:
    struct Pin {
        int x;
        int y;
        std::string label;
    };

    static const std::size_t MAX_PINS = 24;
    static const std::size_t MAX_LABEL_BYTES = 160;

    const std::vector<Pin> &all() const { return pins; }
    bool set(int x, int y, const std::string &label, int width, int height);
    bool remove(int x, int y);
    bool save(const std::string &path) const;
    // A missing file is a valid empty legacy state. Malformed files fail
    // transactionally and leave the existing in-memory pins unchanged.
    bool load(const std::string &path, int width, int height);
    void clear() { pins.clear(); }

private:
    std::vector<Pin> pins;
};

#endif
