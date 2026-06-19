#pragma once

// Project-wide constants. No raylib dependency so this is safe everywhere.

namespace cd::config {

// Internal (logical) render resolution. The world is always drawn at this size
// and then scaled up to the window.
inline constexpr int kVirtualWidth = 426;
inline constexpr int kVirtualHeight = 240;

// Initial window scale and size.
inline constexpr int kDefaultScale = 3;
inline constexpr int kWindowWidth = kVirtualWidth * kDefaultScale;   // 1278
inline constexpr int kWindowHeight = kVirtualHeight * kDefaultScale;  // 720

inline constexpr int kTargetFps = 60;

inline constexpr const char* kWindowTitle = "Crystal Dungeons";

}  // namespace cd::config
