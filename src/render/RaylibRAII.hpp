#pragma once

#include <utility>

#include "raylib.h"

// Move-only RAII owners for raylib GPU resources. These call the matching
// Unload* on destruction, so game code never unloads by hand. They must be
// created after InitWindow() and destroyed before CloseWindow().

namespace cd {

class RenderTextureHandle {
public:
    RenderTextureHandle() = default;
    explicit RenderTextureHandle(RenderTexture2D rt) : rt_(rt), valid_(true) {}
    ~RenderTextureHandle() { reset(); }

    RenderTextureHandle(const RenderTextureHandle&) = delete;
    RenderTextureHandle& operator=(const RenderTextureHandle&) = delete;

    RenderTextureHandle(RenderTextureHandle&& other) noexcept { moveFrom(other); }
    RenderTextureHandle& operator=(RenderTextureHandle&& other) noexcept {
        if (this != &other) {
            reset();
            moveFrom(other);
        }
        return *this;
    }

    bool valid() const { return valid_; }
    const RenderTexture2D& get() const { return rt_; }

    void reset() {
        if (valid_) {
            UnloadRenderTexture(rt_);
            valid_ = false;
        }
    }

private:
    void moveFrom(RenderTextureHandle& other) {
        rt_ = other.rt_;
        valid_ = other.valid_;
        other.valid_ = false;
    }

    RenderTexture2D rt_{};
    bool valid_ = false;
};

class TextureHandle {
public:
    TextureHandle() = default;
    explicit TextureHandle(Texture2D tex) : tex_(tex), valid_(true) {}
    ~TextureHandle() { reset(); }

    TextureHandle(const TextureHandle&) = delete;
    TextureHandle& operator=(const TextureHandle&) = delete;

    TextureHandle(TextureHandle&& other) noexcept { moveFrom(other); }
    TextureHandle& operator=(TextureHandle&& other) noexcept {
        if (this != &other) {
            reset();
            moveFrom(other);
        }
        return *this;
    }

    bool valid() const { return valid_; }
    const Texture2D& get() const { return tex_; }

    void reset() {
        if (valid_) {
            UnloadTexture(tex_);
            valid_ = false;
        }
    }

private:
    void moveFrom(TextureHandle& other) {
        tex_ = other.tex_;
        valid_ = other.valid_;
        other.valid_ = false;
    }

    Texture2D tex_{};
    bool valid_ = false;
};

class FontHandle {
public:
    FontHandle() = default;
    explicit FontHandle(Font font) : font_(font), valid_(true) {}
    ~FontHandle() { reset(); }

    FontHandle(const FontHandle&) = delete;
    FontHandle& operator=(const FontHandle&) = delete;

    FontHandle(FontHandle&& other) noexcept { moveFrom(other); }
    FontHandle& operator=(FontHandle&& other) noexcept {
        if (this != &other) {
            reset();
            moveFrom(other);
        }
        return *this;
    }

    bool valid() const { return valid_; }
    const Font& get() const { return font_; }

    void reset() {
        if (valid_) {
            UnloadFont(font_);
            valid_ = false;
        }
    }

private:
    void moveFrom(FontHandle& other) {
        font_ = other.font_;
        valid_ = other.valid_;
        other.valid_ = false;
    }

    Font font_{};
    bool valid_ = false;
};

}  // namespace cd
