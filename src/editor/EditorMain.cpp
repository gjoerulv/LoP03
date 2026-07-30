// CrystalForge (M59) — the content-editor tool's entry point. A separate
// executable from the game: it opens its own 1280x720 raylib window, renders
// the shell at 640x360 logical resolution into a point-filtered texture and
// blits it 2x (the shipped pixel fonts stay crisp; the M46 kit's metrics work
// unchanged), and edits the SOURCE-TREE data/ so changes land where the owner
// commits from. `--canonicalize` runs headless: it rewrites every data file
// through the canonical writer (the one-time normalization / drift repair)
// and proves the values unchanged by re-validating through the real loader.

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

#include "raylib.h"

#include "editor/EditorDocs.hpp"
#include "editor/EditorShell.hpp"
#include "editor/EditorValidation.hpp"
#include "ui/UiDraw.hpp"

namespace {

namespace fs = std::filesystem;

fs::path resolveDataDir(int argc, char** argv) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string_view(argv[i]) == "--data") {
            return fs::path(argv[i + 1]);
        }
    }
#ifdef CRYSTAL_EDITOR_DATA_DIR
    if (fs::exists(fs::path(CRYSTAL_EDITOR_DATA_DIR) / "skills.json")) {
        return fs::path(CRYSTAL_EDITOR_DATA_DIR);
    }
#endif
    return fs::current_path() / "data";
}

fs::path resolveAssetsDir() {
#ifdef CRYSTAL_EDITOR_ASSETS_DIR
    if (fs::exists(fs::path(CRYSTAL_EDITOR_ASSETS_DIR) / "fonts")) {
        return fs::path(CRYSTAL_EDITOR_ASSETS_DIR);
    }
#endif
    return fs::current_path() / "assets";
}

bool hasFlag(int argc, char** argv, std::string_view flag) {
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == flag) {
            return true;
        }
    }
    return false;
}

// Headless normalization: load -> validate -> canonical rewrite -> reload ->
// re-validate. Exits non-zero when anything failed or the reloaded content
// no longer validates (it must — the writer only reformats).
int runCanonicalize(const fs::path& dataDir) {
    cd::editor::EditorDocs docs;
    if (!docs.loadAll(dataDir)) {
        std::fprintf(stderr, "canonicalize: some files failed to load under %s\n",
                     dataDir.string().c_str());
        for (const cd::editor::CategoryInfo& info : cd::editor::categories()) {
            const cd::editor::DocFile& doc = docs.file(info.category);
            if (!doc.loadError.empty()) {
                std::fprintf(stderr, "  %s: %s\n", info.filename, doc.loadError.c_str());
            }
        }
        return 1;
    }
    const cd::editor::ValidationResult before = cd::editor::validateDocs(docs);
    int changed = 0;
    std::string error;
    if (!docs.canonicalizeAll(changed, error)) {
        std::fprintf(stderr, "canonicalize: write failed: %s\n", error.c_str());
        return 1;
    }
    cd::editor::EditorDocs reloaded;
    if (!reloaded.loadAll(dataDir)) {
        std::fprintf(stderr, "canonicalize: reload failed after writing\n");
        return 1;
    }
    const cd::editor::ValidationResult after = cd::editor::validateDocs(reloaded);
    if (after.report.errorCount() != before.report.errorCount()) {
        std::fprintf(stderr,
                     "canonicalize: error count changed (%zu -> %zu) - values were NOT preserved\n",
                     before.report.errorCount(), after.report.errorCount());
        return 1;
    }
    std::printf("canonicalize: %d file(s) rewritten, %zu content error(s) before and after\n",
                changed, after.report.errorCount());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const fs::path dataDir = resolveDataDir(argc, argv);
    if (!fs::exists(dataDir / "skills.json")) {
        std::fprintf(stderr,
                     "CrystalForge: no content found at %s (pass --data <dir> pointing at the "
                     "repo's data folder)\n",
                     dataDir.string().c_str());
        return 1;
    }

    if (hasFlag(argc, argv, "--canonicalize")) {
        return runCanonicalize(dataDir);
    }

    cd::editor::EditorDocs docs;
    docs.loadAll(dataDir);  // per-file failures surface inside the shell

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(cd::editor::kLogicalW * cd::editor::kWindowScale,
               cd::editor::kLogicalH * cd::editor::kWindowScale, "CrystalForge");
    SetExitKey(0);  // Esc is a UI key; quitting goes through the unsaved guard
    SetTargetFPS(60);

    const fs::path fontsDir = resolveAssetsDir() / "fonts";
    const Font fontSmall = LoadFont((fontsDir / "font_small.fnt").string().c_str());
    const Font fontMain = LoadFont((fontsDir / "font_main.fnt").string().c_str());
    const Font fontTitle = LoadFont((fontsDir / "font_title.fnt").string().c_str());
    cd::ui::setFonts(&fontSmall, &fontMain, &fontTitle);

    RenderTexture2D canvas =
        LoadRenderTexture(cd::editor::kLogicalW, cd::editor::kLogicalH);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_POINT);

    cd::editor::EditorShell shell(docs);
    while (!shell.wantsQuit()) {
        if (WindowShouldClose()) {
            shell.requestWindowClose();
        }
        shell.update();

        BeginTextureMode(canvas);
        shell.render();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(canvas.texture,
                       Rectangle{0, 0, static_cast<float>(canvas.texture.width),
                                 -static_cast<float>(canvas.texture.height)},
                       Rectangle{0, 0,
                                 static_cast<float>(cd::editor::kLogicalW *
                                                    cd::editor::kWindowScale),
                                 static_cast<float>(cd::editor::kLogicalH *
                                                    cd::editor::kWindowScale)},
                       Vector2{0, 0}, 0.0f, WHITE);
        EndDrawing();
    }

    cd::ui::setFonts(nullptr, nullptr, nullptr);
    UnloadRenderTexture(canvas);
    UnloadFont(fontSmall);
    UnloadFont(fontMain);
    UnloadFont(fontTitle);
    CloseWindow();
    return 0;
}
