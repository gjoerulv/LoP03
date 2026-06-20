#include "dungeon/DungeonModel.hpp"

namespace cd::dungeon {

Dir opposite(Dir d) {
    switch (d) {
        case Dir::North: return Dir::South;
        case Dir::East: return Dir::West;
        case Dir::South: return Dir::North;
        case Dir::West: return Dir::East;
    }
    return Dir::North;
}

int dirDx(Dir d) {
    switch (d) {
        case Dir::East: return 1;
        case Dir::West: return -1;
        case Dir::North:
        case Dir::South: return 0;
    }
    return 0;
}

int dirDy(Dir d) {
    switch (d) {
        case Dir::South: return 1;
        case Dir::North: return -1;
        case Dir::East:
        case Dir::West: return 0;
    }
    return 0;
}

int Dungeon::chestCount() const {
    int n = 0;
    for (const Room& r : rooms) {
        if (r.chest.present) {
            ++n;
        }
    }
    return n;
}

int Dungeon::guardedChestCount() const {
    int n = 0;
    for (const Room& r : rooms) {
        if (r.chest.present && r.chest.guarded) {
            ++n;
        }
    }
    return n;
}

}  // namespace cd::dungeon
