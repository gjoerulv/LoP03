#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "editor/EditorDocs.hpp"
#include "editor/EditorValidation.hpp"
#include "editor/SimLab.hpp"
#include "editor/TestRunner.hpp"
#include "platform/Process.hpp"
#include "ui/Menu.hpp"
#include "ui/ScrollWindow.hpp"
#include "ui/TextInput.hpp"

// CrystalForge's windowed shell (M59): four panes — category sidebar, entity
// list, field form, validation messages — over the EditorDocs store, drawn
// with the game's M46 UI kit on a 640x360 logical canvas that EditorMain
// blits 2x into a 1280x720 window (pixel fonts stay crisp; the kit's metrics
// work unchanged). Keyboard-first (Tab between panes, arrows, Enter, Esc,
// Ctrl+S save, F5 validate, N/D/Del on the entity list) with a thin mouse
// layer (click to focus/select, wheel to scroll) that lives entirely here —
// src/ui/ stays mouse-free.

namespace cd::editor {

inline constexpr int kLogicalW = 640;
inline constexpr int kLogicalH = 360;
inline constexpr int kWindowScale = 2;

class EditorShell {
public:
    explicit EditorShell(EditorDocs& docs);

    // One frame of input handling; call before render().
    void update();
    // Draws the whole shell at 640x360 logical coordinates.
    void render();

    bool wantsQuit() const { return quit_; }
    // The window's close button: quit immediately when clean, else raise the
    // save/discard prompt (raylib's close flag cannot be cleared, so this
    // prompt has no cancel — Esc-initiated quits do).
    void requestWindowClose();

private:
    enum class Pane { Categories, Entities, Form, Messages };
    // M60: the sidebar hosts two extra surfaces below the nine categories.
    enum class Screen { Content, SimLab, Tests };
    enum class Modal {
        None,
        TextEdit,      // String/Text field editing
        Pick,          // Enum value or referenced id (single choice)
        ListEdit,      // IdList / EnumList entries
        ArrayEdit,     // ObjectArray entries (learnset, statuses)
        ConfirmDelete,
        ConfirmQuit,
    };

    // One row of the sim lab's config pane (rebuilt per frame; update and
    // render walk the same list so they can never disagree).
    struct SimRow {
        enum class Kind {
            Level, Preset, MemberClass, MemberGear, MemberPassive, Mode, Boss, RushIndex,
            Wave, AddEnemy, RemoveEnemy, Scale, Seeds, Run, ExportMd, ExportCsv
        } kind = Kind::Level;
        int member = 0;  // MemberClass/MemberGear/MemberPassive
        int slot = 0;    // MemberGear: 0 weapon, 1 armor, 2 accessory
        std::string label;
        std::string value;
    };

    // One visible form row: a field, a flattened Object child, a section
    // header, or an unrecognized key.
    struct FormRow {
        enum class Kind { Value, Section, Unknown } kind = Kind::Value;
        const FieldDesc* desc = nullptr;
        const FieldDesc* child = nullptr;  // non-null: value lives in desc's object
        std::string label;
        std::string unknownKey;
    };

    // Messages pane entry: a load error (jumpable) or a quick-check line.
    struct MessageRow {
        std::string text;
        bool isError = false;
        std::string source;   // error file
        std::string context;  // error context (for the jump)
    };

    Category category() const;
    OrderedJson* currentEntity();
    const FormRow* currentFormRow() const;

    void selectCategory(int index);
    void rebuildEntities(bool keepCursor);
    void rebuildForm();
    void rebuildMessages();
    void runValidation();

    // Value plumbing for the focused row.
    OrderedJson rowValue(const FormRow& row);
    void writeRowValue(const FormRow& row, const OrderedJson& value);
    std::string rowValueText(const FormRow& row);
    void stepRow(const FormRow& row, int direction);
    void activateRow(const FormRow& row);

    // Modal openers/handlers.
    void openTextEdit(const FormRow& row);
    void openPick(const FormRow& row);
    void openListEdit(const FormRow& row);
    void openArrayEdit(const FormRow& row);
    void confirmDelete();
    void updateModal();
    void renderModal();

    void handlePaneInput();
    void handleMouse();
    void saveAll();
    void jumpToError(const MessageRow& row);

    // --- M60 sim lab ---
    std::vector<SimRow> buildSimRows() const;
    void applySimPreset();
    void stepSimRow(const SimRow& row, int direction);
    void activateSimRow(const SimRow& row);
    void openSimPick(std::string* target, Category category, const std::string& slotFilter,
                     bool appendEnemy);
    void rebuildSimPickMenu();
    void runSimSweep();
    void exportSimReport(bool markdown);
    void updateSimLab();
    void renderSimLab();

    // --- M60 test runner ---
    void startTestRun(int categoryIndex);
    void updateTests();
    void renderTests();

    EditorDocs& docs_;
    bool quit_ = false;
    Pane pane_ = Pane::Categories;
    Screen screen_ = Screen::Content;
    Modal modal_ = Modal::None;

    ui::Menu categoryMenu_;
    ui::Menu entityMenu_;
    ui::ScrollWindow entityWindow_;
    bool entitiesStale_ = true;

    std::vector<FormRow> formRows_;
    int formCursor_ = 0;
    ui::ScrollWindow formWindow_;

    std::vector<MessageRow> messages_;
    int messageCursor_ = 0;
    ui::ScrollWindow messageWindow_;
    std::optional<ValidationResult> validation_;

    std::string status_;  // one-line result of the last save/validate/action

    // --- modal state ---
    ui::TextInput textEdit_{240, "", ui::TextFilter::Printable};
    FormRow modalRow_;                  // the row a modal is editing
    ui::Menu pickMenu_;
    ui::ScrollWindow pickWindow_;
    std::vector<std::string> pickValues_;  // parallel to pickMenu_ ("" = clear)
    std::string pickFilter_;
    ui::Menu listMenu_;
    ui::ScrollWindow listWindow_;
    bool listPicking_ = false;  // ListEdit's nested append-picker
    ui::Menu arrayMenu_;
    ui::ScrollWindow arrayWindow_;
    bool arrayEditingEntry_ = false;
    int arrayFieldCursor_ = 0;
    std::string confirmText_;
    bool quitCancellable_ = true;

    // --- M60 sim lab state ---
    SimLabConfig simConfig_;
    GearTier simPreset_ = GearTier::Median;
    std::optional<SimLabResult> simResult_;
    std::optional<SimLabResult> simPrevious_;  // delta baseline
    int simCursor_ = 0;
    ui::ScrollWindow simWindow_;
    // The Pick modal doubles as the sim lab's picker: when simPickTarget_ (or
    // the append flag) is set, a chosen id lands there instead of a document.
    std::string* simPickTarget_ = nullptr;
    bool simPickAppendEnemy_ = false;
    Category simPickCategory_ = Category::Skills;
    std::string simPickSlotFilter_;  // items only: "weapon"/"armor"/"accessory"

    // --- M60 test runner state ---
    int testCursor_ = 0;
    int testRunningCategory_ = -1;  // -1 = idle
    std::unique_ptr<platform::ProcessRunner> testRunner_;
    std::vector<std::string> testOutput_;
    ui::ScrollWindow testWindow_;
    std::string testSummary_;
};

}  // namespace cd::editor
