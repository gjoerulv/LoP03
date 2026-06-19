#include "render/Viewport.hpp"

namespace cd {

ViewportRect computeViewport(int windowW, int windowH, int internalW, int internalH) {
    ViewportRect out;
    if (windowW <= 0 || windowH <= 0 || internalW <= 0 || internalH <= 0) {
        return out;  // all zeros
    }

    const float scaleX = static_cast<float>(windowW) / static_cast<float>(internalW);
    const float scaleY = static_cast<float>(windowH) / static_cast<float>(internalH);
    const float scale = scaleX < scaleY ? scaleX : scaleY;

    out.scale = scale;
    out.width = static_cast<float>(internalW) * scale;
    out.height = static_cast<float>(internalH) * scale;
    out.x = (static_cast<float>(windowW) - out.width) * 0.5f;
    out.y = (static_cast<float>(windowH) - out.height) * 0.5f;
    return out;
}

}  // namespace cd
