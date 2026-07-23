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

class SoundHandle {
public:
    SoundHandle() = default;
    explicit SoundHandle(Sound sound) : sound_(sound), valid_(true) {}
    ~SoundHandle() { reset(); }

    SoundHandle(const SoundHandle&) = delete;
    SoundHandle& operator=(const SoundHandle&) = delete;

    SoundHandle(SoundHandle&& other) noexcept { moveFrom(other); }
    SoundHandle& operator=(SoundHandle&& other) noexcept {
        if (this != &other) {
            reset();
            moveFrom(other);
        }
        return *this;
    }

    bool valid() const { return valid_; }
    const Sound& get() const { return sound_; }

    void reset() {
        if (valid_) {
            UnloadSound(sound_);
            valid_ = false;
        }
    }

private:
    void moveFrom(SoundHandle& other) {
        sound_ = other.sound_;
        valid_ = other.valid_;
        other.valid_ = false;
    }

    Sound sound_{};
    bool valid_ = false;
};

class MusicHandle {
public:
    MusicHandle() = default;
    explicit MusicHandle(Music music) : music_(music), valid_(true) {}
    ~MusicHandle() { reset(); }

    MusicHandle(const MusicHandle&) = delete;
    MusicHandle& operator=(const MusicHandle&) = delete;

    MusicHandle(MusicHandle&& other) noexcept { moveFrom(other); }
    MusicHandle& operator=(MusicHandle&& other) noexcept {
        if (this != &other) {
            reset();
            moveFrom(other);
        }
        return *this;
    }

    bool valid() const { return valid_; }
    Music& get() { return music_; }  // non-const: raylib mutates stream state
    const Music& get() const { return music_; }

    void reset() {
        if (valid_) {
            UnloadMusicStream(music_);
            valid_ = false;
        }
    }

private:
    void moveFrom(MusicHandle& other) {
        music_ = other.music_;
        valid_ = other.valid_;
        other.valid_ = false;
    }

    Music music_{};
    bool valid_ = false;
};

class ShaderHandle {
public:
    ShaderHandle() = default;
    explicit ShaderHandle(Shader shader) : shader_(shader), valid_(true) {}
    ~ShaderHandle() { reset(); }

    ShaderHandle(const ShaderHandle&) = delete;
    ShaderHandle& operator=(const ShaderHandle&) = delete;

    ShaderHandle(ShaderHandle&& other) noexcept { moveFrom(other); }
    ShaderHandle& operator=(ShaderHandle&& other) noexcept {
        if (this != &other) {
            reset();
            moveFrom(other);
        }
        return *this;
    }

    bool valid() const { return valid_; }
    const Shader& get() const { return shader_; }

    void reset() {
        if (valid_) {
            UnloadShader(shader_);
            valid_ = false;
        }
    }

private:
    void moveFrom(ShaderHandle& other) {
        shader_ = other.shader_;
        valid_ = other.valid_;
        other.valid_ = false;
    }

    Shader shader_{};
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
