#pragma once

// M126: packs a run of fixed-width tags (an icon + a name each) into lines,
// left to right. A tag that does not fit what is left of the line starts the
// next one; a lone tag wider than the whole line keeps a line to itself (the
// draw site fits it). Pure - the widths come from the caller's measure - so
// the victory panel can size its frame from the same answer it draws.

#include <cstddef>
#include <vector>

namespace cd::ui {

struct TagFlowLine {
    std::size_t first = 0;  // index of the line's first tag
    std::size_t count = 0;  // tags on the line (>= 1)
};

inline std::vector<TagFlowLine> flowTags(const std::vector<int>& widths, int gap, int maxWidth) {
    std::vector<TagFlowLine> lines;
    std::size_t i = 0;
    while (i < widths.size()) {
        TagFlowLine line{i, 0};
        int used = 0;
        while (i < widths.size()) {
            const int need = (line.count > 0 ? gap : 0) + widths[i];
            if (line.count > 0 && used + need > maxWidth) {
                break;
            }
            used += need;
            ++line.count;
            ++i;
        }
        lines.push_back(line);
    }
    return lines;
}

}  // namespace cd::ui
