#pragma once

// Pure scaling math for fitting the internal resolution inside a window while
// preserving aspect ratio (letterbox/pillarbox). Deliberately free of raylib so
// it can be unit-tested headlessly.

namespace cd {

struct ViewportRect {
    float x = 0.0f;       // left offset inside the window (letterbox bar width)
    float y = 0.0f;       // top offset inside the window (pillarbox bar height)
    float width = 0.0f;   // scaled width
    float height = 0.0f;  // scaled height
    float scale = 0.0f;   // uniform scale factor applied to the internal size
};

// Compute the largest aspect-preserving rectangle of size (internalW x internalH)
// that fits inside (windowW x windowH), centered. Returns all-zero on degenerate
// input (non-positive dimensions).
ViewportRect computeViewport(int windowW, int windowH, int internalW, int internalH);

}  // namespace cd
