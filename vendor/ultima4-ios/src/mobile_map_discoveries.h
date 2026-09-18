#ifndef ZU4_MOBILE_MAP_DISCOVERIES_H
#define ZU4_MOBILE_MAP_DISCOVERIES_H

#include <string>
#include <vector>

class MobileMapDiscoveries {
public:
    enum Category {
        TOWN = 1,
        CASTLE,
        VILLAGE,
        SHRINE,
        DUNGEON
    };

    struct Place {
        int x;
        int y;
        Category category;
        std::string name;
    };

    static const std::size_t MAX_PLACES = 40;
    static const std::size_t MAX_NAME_BYTES = 160;

    const std::vector<Place> &all() const { return places; }
    bool discover(int x, int y, Category category, const std::string &name,
                  int width, int height);
    bool save(const std::string &path) const;
    // Missing metadata is a valid empty state for checkpoints created before
    // discovered-place markers. Invalid input leaves live discoveries intact.
    bool load(const std::string &path, int width, int height);
    void clear() { places.clear(); }

private:
    std::vector<Place> places;
};

#endif
