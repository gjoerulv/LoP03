#include "town/Movement.hpp"

namespace cd::town {

Vec2 resolveMove(const Rect& body, float dx, float dy, const Tilemap& map) {
    Rect r = body;

    r.x = body.x + dx;
    if (map.rectHitsSolid(r)) {
        r.x = body.x;  // horizontal blocked
    }

    r.y = body.y + dy;
    if (map.rectHitsSolid(r)) {
        r.y = body.y;  // vertical blocked
    }

    return Vec2{r.x, r.y};
}

}  // namespace cd::town
