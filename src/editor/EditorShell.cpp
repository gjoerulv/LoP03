#include "editor/EditorShell.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <utility>

#include "raylib.h"
#include "game/Party.hpp"  // kMaxLevel for the sim lab's level stepper
#include "platform/AtomicFile.hpp"
#include "ui/UiDraw.hpp"
#include "ui/UiStyle.hpp"

namespace cd::editor {

namespace {

// --- layout (640x360 logical) ----------------------------------------------
constexpr int kTop = 28;                       // below the header band
constexpr int kBottom = kLogicalH - ui::style::kFooterHeight - 4;  // 340
constexpr int kPaneH = kBottom - kTop;         // 312

constexpr int kSideX = 4, kSideW = 100;
constexpr int kEntX = 108, kEntW = 170;
constexpr int kFormX = 282, kFormW = 210;
constexpr int kMsgX = 496, kMsgW = 140;

constexpr int kRowH = 14;
constexpr int kFormRowH = 13;
constexpr int kMsgRowH = 10;
constexpr int kEntityRows = 20;
constexpr int kFormRowsVisible = 22;
constexpr int kMsgRows = 28;
constexpr int kFormValueX = 118;  // value column offset inside the form pane

bool keyPressed(int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); }
bool ctrlDown() { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL); }

std::string joinStrings(const OrderedJson& arr) {
    std::string out;
    for (const OrderedJson& el : arr) {
        if (!out.empty()) {
            out += ", ";
        }
        out += el.is_string() ? el.get<std::string>() : el.dump();
    }
    return out;
}

// "skill=shield_bash lv=4" — a compact one-line label for an array entry.
std::string entrySummary(const OrderedJson& entry, const std::vector<FieldDesc>& children) {
    std::string out;
    for (const FieldDesc& child : children) {
        const OrderedJson v = fieldValue(entry, child);
        if (!out.empty()) {
            out += "  ";
        }
        out += child.key + "=" + (v.is_string() ? v.get<std::string>() : v.dump());
    }
    return out.empty() ? "(empty)" : out;
}

// Step a numeric value by whole descriptor steps, staying on the grid.
OrderedJson steppedValue(const FieldDesc& desc, const OrderedJson& current, int direction) {
    const double cur = current.is_number() ? current.get<double>() : desc.defaultNumber;
    const double stepped = cur + direction * desc.step;
    const double clamped = std::clamp(stepped, desc.minValue, desc.maxValue);
    if (desc.kind == FieldKind::Float) {
        // Keep growth curves on tidy tenths (0.1 steps drift in binary floats).
        return OrderedJson(std::round(clamped * 10.0) / 10.0);
    }
    return OrderedJson(static_cast<long long>(std::llround(clamped)));
}

}  // namespace

namespace {

// Sidebar rows: every category, then the two M60 surfaces. kCategoryCount is
// pinned to categories().size() by the [editor] suite, so these two indices
// can never drift onto a category row again (the M63..M85 Story regression).
constexpr int kSidebarSimLab = kCategoryCount;
constexpr int kSidebarTests = kCategoryCount + 1;

}  // namespace

EditorShell::EditorShell(EditorDocs& docs) : docs_(docs) {
    simConfig_.members.resize(4);
    simConfig_.level = 20;
    selectCategory(0);
    applySimPreset();
    runValidation();
}

Category EditorShell::category() const {
    const int cursor = std::min(categoryMenu_.cursor(), kCategoryCount - 1);
    return categories()[static_cast<std::size_t>(cursor)].category;
}

OrderedJson* EditorShell::currentEntity() {
    return docs_.entityAt(category(), entityMenu_.cursor());
}

const EditorShell::FormRow* EditorShell::currentFormRow() const {
    if (formCursor_ < 0 || formCursor_ >= static_cast<int>(formRows_.size())) {
        return nullptr;
    }
    return &formRows_[static_cast<std::size_t>(formCursor_)];
}

void EditorShell::selectCategory(int index) {
    std::vector<ui::MenuItem> items;
    for (const CategoryInfo& info : categories()) {
        items.push_back({info.title, true, docs_.file(info.category).dirty ? "*" : ""});
    }
    items.push_back({"Sim Lab", true, ""});
    items.push_back({"Test Runner", true, ""});
    categoryMenu_.setItems(std::move(items));
    categoryMenu_.setCursor(index);
    if (index == kSidebarSimLab) {
        screen_ = Screen::SimLab;
        return;
    }
    if (index == kSidebarTests) {
        screen_ = Screen::Tests;
        return;
    }
    screen_ = Screen::Content;
    entityMenu_.setCursor(0);
    entityWindow_.reset();
    entitiesStale_ = true;
    rebuildEntities(false);
    rebuildForm();
}

void EditorShell::rebuildEntities(bool keepCursor) {
    const int cursor = keepCursor ? entityMenu_.cursor() : 0;
    std::vector<ui::MenuItem> items;
    const int count = docs_.entityCount(category());
    for (int i = 0; i < count; ++i) {
        items.push_back({docs_.entityLabel(category(), i), true, docs_.entitySuffix(category(), i)});
    }
    if (items.empty()) {
        items.push_back({docs_.file(category()).loaded ? "(empty)" : "(file failed to load)",
                         false, ""});
    }
    entityMenu_.setItems(std::move(items));
    entityMenu_.setCursor(cursor);
    entitiesStale_ = false;
}

void EditorShell::rebuildForm() {
    formRows_.clear();
    const OrderedJson* entity = docs_.entityAt(category(), entityMenu_.cursor());
    const std::vector<FieldDesc>& descs = descriptorsFor(category());
    for (const FieldDesc& desc : descs) {
        if (desc.kind == FieldKind::Object) {
            FormRow section;
            section.kind = FormRow::Kind::Section;
            section.desc = &desc;
            section.label = desc.label;
            formRows_.push_back(section);
            for (const FieldDesc& child : desc.children) {
                FormRow row;
                row.desc = &desc;
                row.child = &child;
                row.label = "  " + child.label;
                formRows_.push_back(row);
            }
            continue;
        }
        FormRow row;
        row.desc = &desc;
        row.label = desc.label;
        formRows_.push_back(row);
    }
    if (entity != nullptr) {
        for (const std::string& key : unrecognizedKeys(*entity, descs)) {
            if (key == "version") {
                continue;  // the root schema tag on composition/story roots
            }
            FormRow row;
            row.kind = FormRow::Kind::Unknown;
            row.unknownKey = key;
            row.label = key + " (unrecognized)";
            formRows_.push_back(row);
        }
    }
    formCursor_ = 0;
    while (formCursor_ < static_cast<int>(formRows_.size()) &&
           formRows_[static_cast<std::size_t>(formCursor_)].kind == FormRow::Kind::Section) {
        ++formCursor_;
    }
    formWindow_.reset();
}

// --- value plumbing ---------------------------------------------------------

OrderedJson EditorShell::rowValue(const FormRow& row) {
    OrderedJson* entity = currentEntity();
    if (entity == nullptr || row.desc == nullptr) {
        return OrderedJson();
    }
    return row.child != nullptr ? childValue(*entity, *row.desc, *row.child)
                                : fieldValue(*entity, *row.desc);
}

void EditorShell::writeRowValue(const FormRow& row, const OrderedJson& value) {
    OrderedJson* entity = currentEntity();
    if (entity == nullptr || row.desc == nullptr) {
        return;
    }
    if (row.child != nullptr) {
        setChildValue(*entity, *row.desc, *row.child, value);
    } else {
        setFieldValue(*entity, *row.desc, value);
    }
    docs_.markDirty(category());
    entitiesStale_ = true;  // id/name/title edits change list labels
}

std::string EditorShell::rowValueText(const FormRow& row) {
    if (row.kind == FormRow::Kind::Unknown) {
        return "(kept as-is)";
    }
    const FieldDesc& desc = row.child != nullptr ? *row.child : *row.desc;
    const OrderedJson v = rowValue(row);
    switch (desc.kind) {
        case FieldKind::Int:
        case FieldKind::Float:
            return v.is_number() ? v.dump() : "?";
        case FieldKind::Bool:
            return v.is_boolean() && v.get<bool>() ? "on" : "off";
        case FieldKind::Enum:
        case FieldKind::IdRef: {
            const std::string s = v.is_string() ? v.get<std::string>() : "";
            return s.empty() ? "(none)" : s;
        }
        case FieldKind::String:
        case FieldKind::Text: {
            const std::string s = v.is_string() ? v.get<std::string>() : "";
            return s.empty() ? "-" : s;
        }
        case FieldKind::IdList:
        case FieldKind::EnumList:
            return v.is_array() && !v.empty() ? joinStrings(v) : "(none)";
        case FieldKind::ObjectArray:
            return v.is_array()
                       ? std::to_string(v.size()) + (v.size() == 1 ? " entry" : " entries")
                       : "?";
        case FieldKind::Object:
            return "";
    }
    return "";
}

// Candidate (value, suffix) pairs for a pick: enum ids or another category's
// entity ids. Reads the live documents, so a just-added skill is offerable.
static std::vector<std::pair<std::string, std::string>> pickCandidates(
    const EditorDocs& docs, const FieldDesc& desc) {
    std::vector<std::pair<std::string, std::string>> out;
    if (desc.kind == FieldKind::Enum || desc.kind == FieldKind::EnumList) {
        for (const std::string& v : desc.enumValues) {
            out.emplace_back(v, "");
        }
        return out;
    }
    const int count = docs.entityCount(desc.refCategory);
    for (int i = 0; i < count; ++i) {
        out.emplace_back(docs.entityLabel(desc.refCategory, i),
                         docs.entitySuffix(desc.refCategory, i));
    }
    std::sort(out.begin(), out.end());
    return out;
}

void EditorShell::stepRow(const FormRow& row, int direction) {
    if (row.kind != FormRow::Kind::Value) {
        return;
    }
    const FieldDesc& desc = row.child != nullptr ? *row.child : *row.desc;
    switch (desc.kind) {
        case FieldKind::Int:
        case FieldKind::Float:
            writeRowValue(row, steppedValue(desc, rowValue(row), direction));
            break;
        case FieldKind::Bool: {
            const OrderedJson v = rowValue(row);
            writeRowValue(row, OrderedJson(!(v.is_boolean() && v.get<bool>())));
            break;
        }
        case FieldKind::Enum:
        case FieldKind::IdRef: {
            std::vector<std::string> options;
            if (!desc.required) {
                options.push_back(desc.kind == FieldKind::Enum ? desc.defaultString
                                                               : std::string());
            }
            for (const auto& [value, suffix] : pickCandidates(docs_, desc)) {
                if (options.empty() || value != options.front()) {
                    options.push_back(value);
                }
            }
            if (options.empty()) {
                break;
            }
            const OrderedJson v = rowValue(row);
            const std::string current = v.is_string() ? v.get<std::string>() : "";
            int index = 0;
            for (std::size_t i = 0; i < options.size(); ++i) {
                if (options[i] == current) {
                    index = static_cast<int>(i);
                    break;
                }
            }
            const int n = static_cast<int>(options.size());
            index = (index + direction % n + n) % n;
            writeRowValue(row, OrderedJson(options[static_cast<std::size_t>(index)]));
            break;
        }
        default:
            break;
    }
}

void EditorShell::activateRow(const FormRow& row) {
    if (row.kind != FormRow::Kind::Value) {
        return;
    }
    const FieldDesc& desc = row.child != nullptr ? *row.child : *row.desc;
    switch (desc.kind) {
        case FieldKind::String:
        case FieldKind::Text:
            openTextEdit(row);
            break;
        case FieldKind::Enum:
        case FieldKind::IdRef:
            openPick(row);
            break;
        case FieldKind::IdList:
        case FieldKind::EnumList:
            openListEdit(row);
            break;
        case FieldKind::ObjectArray:
            openArrayEdit(row);
            break;
        case FieldKind::Bool:
            stepRow(row, 1);
            break;
        default:
            break;
    }
}

// --- modals -----------------------------------------------------------------

void EditorShell::openTextEdit(const FormRow& row) {
    modalRow_ = row;
    const OrderedJson v = rowValue(row);
    textEdit_.setValue(v.is_string() ? v.get<std::string>() : "");
    modal_ = Modal::TextEdit;
}

// (Re)builds the pick menu from the filter; shared by Pick and ListEdit's
// nested append-picker. `exclude` drops values already present in a list.
static void buildPickMenu(const EditorDocs& docs, const FieldDesc& desc,
                          const std::string& filter, const std::vector<std::string>& exclude,
                          bool offerClear, ui::Menu& menu, std::vector<std::string>& values) {
    values.clear();
    std::vector<ui::MenuItem> items;
    if (offerClear) {
        values.emplace_back();
        items.push_back({"(clear)", true, ""});
    }
    for (const auto& [value, suffix] : pickCandidates(docs, desc)) {
        if (!filter.empty() && value.find(filter) == std::string::npos) {
            continue;
        }
        if (std::find(exclude.begin(), exclude.end(), value) != exclude.end()) {
            continue;
        }
        values.push_back(value);
        items.push_back({value, true, suffix});
    }
    if (items.empty()) {
        items.push_back({"(no match)", false, ""});
    }
    menu.setItems(std::move(items));
}

void EditorShell::openPick(const FormRow& row) {
    modalRow_ = row;
    pickFilter_.clear();
    const FieldDesc& desc = row.child != nullptr ? *row.child : *row.desc;
    buildPickMenu(docs_, desc, pickFilter_, {}, !desc.required, pickMenu_, pickValues_);
    pickWindow_.reset();
    modal_ = Modal::Pick;
}

void EditorShell::openListEdit(const FormRow& row) {
    modalRow_ = row;
    listPicking_ = false;
    const OrderedJson v = rowValue(row);
    std::vector<ui::MenuItem> items;
    if (v.is_array()) {
        for (const OrderedJson& el : v) {
            items.push_back({el.is_string() ? el.get<std::string>() : el.dump(), true, ""});
        }
    }
    if (items.empty()) {
        items.push_back({"(empty)", false, ""});
    }
    listMenu_.setItems(std::move(items));
    listWindow_.reset();
    modal_ = Modal::ListEdit;
}

void EditorShell::openArrayEdit(const FormRow& row) {
    modalRow_ = row;
    arrayEditingEntry_ = false;
    arrayFieldCursor_ = 0;
    const OrderedJson v = rowValue(row);
    std::vector<ui::MenuItem> items;
    if (v.is_array()) {
        for (const OrderedJson& el : v) {
            items.push_back({entrySummary(el, row.desc->children), true, ""});
        }
    }
    if (items.empty()) {
        items.push_back({"(empty)", false, ""});
    }
    arrayMenu_.setItems(std::move(items));
    arrayWindow_.reset();
    modal_ = Modal::ArrayEdit;
}

void EditorShell::confirmDelete() {
    const Category cat = category();
    if (cat == Category::Composition || docs_.entityCount(cat) == 0) {
        return;
    }
    const std::string id = docs_.entityLabel(cat, entityMenu_.cursor());
    const int refs = docs_.referenceCount(id, cat, entityMenu_.cursor());
    confirmText_ = "Delete '" + id + "'?";
    if (refs > 0) {
        confirmText_ += " It is referenced " + std::to_string(refs) +
                        " time(s) elsewhere - deleting will break those references.";
    }
    modal_ = Modal::ConfirmDelete;
}

void EditorShell::requestWindowClose() {
    if (!docs_.anyDirty()) {
        quit_ = true;
        return;
    }
    quitCancellable_ = false;
    confirmText_ = "Unsaved changes.";
    modal_ = Modal::ConfirmQuit;
}

void EditorShell::saveAll() {
    std::string error;
    if (docs_.saveAllDirty(error)) {
        status_ = "Saved.";
        runValidation();
    } else {
        status_ = "SAVE FAILED: " + error;
    }
}

void EditorShell::runValidation() {
    validation_ = validateDocs(docs_);
    rebuildMessages();
    if (validation_->databaseOk) {
        int failed = 0;
        for (const QuickCheckResult& check : validation_->checks) {
            if (check.ran && !check.passed) {
                ++failed;
            }
        }
        status_ = failed == 0 ? "Content OK; quick checks pass."
                              : "Content OK; " + std::to_string(failed) + " quick check(s) FAIL.";
    } else {
        status_ = std::to_string(validation_->report.errorCount()) + " content error(s).";
    }
}

void EditorShell::rebuildMessages() {
    messages_.clear();
    if (!validation_) {
        return;
    }
    for (const content::LoadError& error : validation_->report.errors()) {
        MessageRow row;
        row.isError = true;
        row.source = error.source;
        row.context = error.context;
        row.text = error.source + ": " + error.context + ": " + error.message;
        messages_.push_back(row);
    }
    for (const QuickCheckResult& check : validation_->checks) {
        MessageRow row;
        row.text = std::string(check.ran ? (check.passed ? "[ok] " : "[!!] ") : "[--] ") +
                   check.name + " - " + check.detail;
        messages_.push_back(row);
    }
    messageCursor_ = 0;
    messageWindow_.reset();
}

void EditorShell::jumpToError(const MessageRow& row) {
    if (!row.isError) {
        return;
    }
    for (std::size_t i = 0; i < categories().size(); ++i) {
        if (categories()[i].filename == row.source) {
            selectCategory(static_cast<int>(i));
            break;
        }
    }
    // "skills[2] 'fireball'.power" -> entity by quoted id (fallback: [index]).
    const std::size_t q1 = row.context.find('\'');
    const std::size_t q2 = q1 == std::string::npos ? q1 : row.context.find('\'', q1 + 1);
    if (q1 != std::string::npos && q2 != std::string::npos) {
        const std::string id = row.context.substr(q1 + 1, q2 - q1 - 1);
        for (int i = 0; i < docs_.entityCount(category()); ++i) {
            if (docs_.entityLabel(category(), i) == id) {
                entityMenu_.setCursor(i);
                break;
            }
        }
    } else {
        const std::size_t b1 = row.context.find('[');
        const std::size_t b2 = b1 == std::string::npos ? b1 : row.context.find(']', b1);
        if (b1 != std::string::npos && b2 != std::string::npos) {
            entityMenu_.setCursor(std::atoi(row.context.substr(b1 + 1, b2 - b1 - 1).c_str()));
        }
    }
    rebuildForm();
    // Focus the named field when the context ends ".key".
    const std::size_t dot = row.context.rfind('.');
    if (dot != std::string::npos) {
        const std::string key = row.context.substr(dot + 1);
        for (std::size_t i = 0; i < formRows_.size(); ++i) {
            const FormRow& formRow = formRows_[i];
            const FieldDesc* desc = formRow.child != nullptr ? formRow.child : formRow.desc;
            if (formRow.kind == FormRow::Kind::Value && desc != nullptr && desc->key == key) {
                formCursor_ = static_cast<int>(i);
                break;
            }
        }
    }
    pane_ = Pane::Form;
}

// --- input ------------------------------------------------------------------

void EditorShell::update() {
    updateTests();  // drain a running test process regardless of focus/modal
    if (modal_ != Modal::None) {
        updateModal();
        return;
    }
    handleMouse();
    handlePaneInput();
}

void EditorShell::handlePaneInput() {
    if (ctrlDown() && IsKeyPressed(KEY_S)) {
        saveAll();
        return;
    }
    if (IsKeyPressed(KEY_F5)) {
        runValidation();
        return;
    }
    if (IsKeyPressed(KEY_TAB)) {
        const int dir = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? -1 : 1;
        if (screen_ == Screen::Content) {
            pane_ = static_cast<Pane>((static_cast<int>(pane_) + dir + 4) % 4);
        } else {
            // The M60 screens have one main pane; Tab toggles sidebar <-> main.
            pane_ = pane_ == Pane::Categories ? Pane::Entities : Pane::Categories;
        }
        return;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (pane_ == Pane::Form) {
            pane_ = Pane::Entities;
        } else if (pane_ == Pane::Entities || pane_ == Pane::Messages) {
            pane_ = Pane::Categories;
        } else if (!docs_.anyDirty()) {
            quit_ = true;
        } else {
            quitCancellable_ = true;
            confirmText_ = "Unsaved changes.";
            modal_ = Modal::ConfirmQuit;
        }
        return;
    }

    if (screen_ != Screen::Content && pane_ != Pane::Categories) {
        if (screen_ == Screen::SimLab) {
            updateSimLab();
        } else {
            // Test-runner keys (draining happens in update() every frame).
            const int count = static_cast<int>(testCategories().size());
            if (keyPressed(KEY_UP) && testCursor_ > 0) {
                --testCursor_;
            }
            if (keyPressed(KEY_DOWN) && testCursor_ + 1 < count) {
                ++testCursor_;
            }
            if (IsKeyPressed(KEY_ENTER)) {
                startTestRun(testCursor_);
            }
            if (IsKeyPressed(KEY_DELETE) && testRunner_ != nullptr && testRunner_->running()) {
                testRunner_.reset();  // terminates the child
                testRunningCategory_ = -1;
                testSummary_ = "Stopped.";
            }
        }
        return;
    }

    switch (pane_) {
        case Pane::Categories: {
            if (keyPressed(KEY_UP)) {
                categoryMenu_.moveUp();
                selectCategory(categoryMenu_.cursor());
            }
            if (keyPressed(KEY_DOWN)) {
                categoryMenu_.moveDown();
                selectCategory(categoryMenu_.cursor());
            }
            if (IsKeyPressed(KEY_ENTER) || keyPressed(KEY_RIGHT)) {
                pane_ = Pane::Entities;
            }
            break;
        }
        case Pane::Entities: {
            if (keyPressed(KEY_UP)) {
                entityMenu_.moveUp();
                rebuildForm();
            }
            if (keyPressed(KEY_DOWN)) {
                entityMenu_.moveDown();
                rebuildForm();
            }
            if (keyPressed(KEY_PAGE_UP) || keyPressed(KEY_PAGE_DOWN)) {
                const int jump = keyPressed(KEY_PAGE_UP) ? -kEntityRows : kEntityRows;
                entityMenu_.setCursor(entityMenu_.cursor() + jump);
                rebuildForm();
            }
            if (IsKeyPressed(KEY_ENTER) || keyPressed(KEY_RIGHT)) {
                pane_ = Pane::Form;
            }
            if (keyPressed(KEY_LEFT)) {
                pane_ = Pane::Categories;
            }
            if (IsKeyPressed(KEY_N) && category() != Category::Composition) {
                const int index = docs_.addEntity(category());
                if (index >= 0) {
                    rebuildEntities(false);
                    entityMenu_.setCursor(index);
                    rebuildForm();
                    status_ = "Added " + docs_.entityLabel(category(), index) + ".";
                }
            }
            if (IsKeyPressed(KEY_D) && category() != Category::Composition) {
                const int index = docs_.duplicateEntity(category(), entityMenu_.cursor());
                if (index >= 0) {
                    rebuildEntities(false);
                    entityMenu_.setCursor(index);
                    rebuildForm();
                    status_ = "Duplicated as " + docs_.entityLabel(category(), index) + ".";
                }
            }
            if (IsKeyPressed(KEY_DELETE)) {
                confirmDelete();
            }
            break;
        }
        case Pane::Form: {
            const int rowCount = static_cast<int>(formRows_.size());
            auto move = [&](int dir) {
                int next = formCursor_;
                do {
                    next += dir;
                } while (next >= 0 && next < rowCount &&
                         formRows_[static_cast<std::size_t>(next)].kind ==
                             FormRow::Kind::Section);
                if (next >= 0 && next < rowCount) {
                    formCursor_ = next;
                }
            };
            if (keyPressed(KEY_UP)) {
                move(-1);
            }
            if (keyPressed(KEY_DOWN)) {
                move(1);
            }
            if (const FormRow* row = currentFormRow()) {
                if (keyPressed(KEY_LEFT)) {
                    stepRow(*row, -1);
                }
                if (keyPressed(KEY_RIGHT)) {
                    stepRow(*row, 1);
                }
                if (IsKeyPressed(KEY_ENTER)) {
                    activateRow(*row);
                }
            }
            break;
        }
        case Pane::Messages: {
            const int count = static_cast<int>(messages_.size());
            if (keyPressed(KEY_UP) && messageCursor_ > 0) {
                --messageCursor_;
            }
            if (keyPressed(KEY_DOWN) && messageCursor_ + 1 < count) {
                ++messageCursor_;
            }
            if (IsKeyPressed(KEY_ENTER) && messageCursor_ < count) {
                jumpToError(messages_[static_cast<std::size_t>(messageCursor_)]);
            }
            break;
        }
    }
}

void EditorShell::handleMouse() {
    const Vector2 mouse = GetMousePosition();
    const int mx = static_cast<int>(mouse.x) / kWindowScale;
    const int my = static_cast<int>(mouse.y) / kWindowScale;
    const float wheel = GetMouseWheelMove();
    const bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (wheel == 0.0f && !click) {
        return;
    }
    auto inPane = [&](int x, int w) { return mx >= x && mx < x + w && my >= kTop && my < kBottom; };

    if (inPane(kSideX, kSideW)) {
        if (click) {
            const int row = (my - (kTop + 8)) / kRowH;
            if (row >= 0 && row < static_cast<int>(categoryMenu_.size())) {
                pane_ = Pane::Categories;
                selectCategory(row);
            }
        }
        return;
    }
    if (screen_ != Screen::Content) {
        // The M60 screens are keyboard-driven; the wheel still scrolls their
        // main list.
        if (wheel != 0.0f && screen_ == Screen::SimLab) {
            simWindow_.scrollBy(static_cast<int>(buildSimRows().size()), kFormRowsVisible,
                                wheel > 0 ? -3 : 3);
        }
        if (wheel != 0.0f && screen_ == Screen::Tests) {
            testWindow_.scrollBy(static_cast<int>(testOutput_.size()), kMsgRows,
                                 wheel > 0 ? -3 : 3);
        }
        return;
    }
    if (inPane(kEntX, kEntW)) {
        if (wheel != 0.0f) {
            entityWindow_.scrollBy(static_cast<int>(entityMenu_.size()), kEntityRows,
                                   wheel > 0 ? -3 : 3);
        }
        if (click) {
            const int row = (my - (kTop + 8)) / kRowH;
            const int index = entityWindow_.top() + row;
            if (row >= 0 && index < static_cast<int>(entityMenu_.size())) {
                pane_ = Pane::Entities;
                entityMenu_.setCursor(index);
                rebuildForm();
            }
        }
    } else if (inPane(kFormX, kFormW)) {
        if (wheel != 0.0f) {
            formWindow_.scrollBy(static_cast<int>(formRows_.size()), kFormRowsVisible,
                                 wheel > 0 ? -3 : 3);
        }
        if (click) {
            const int row = (my - (kTop + 8)) / kFormRowH;
            const int index = formWindow_.top() + row;
            if (row >= 0 && index < static_cast<int>(formRows_.size()) &&
                formRows_[static_cast<std::size_t>(index)].kind != FormRow::Kind::Section) {
                pane_ = Pane::Form;
                formCursor_ = index;
            }
        }
    } else if (inPane(kMsgX, kMsgW)) {
        if (wheel != 0.0f) {
            messageWindow_.scrollBy(static_cast<int>(messages_.size()), kMsgRows,
                                    wheel > 0 ? -3 : 3);
        }
        if (click) {
            const int row = (my - (kTop + 20)) / kMsgRowH;
            const int index = messageWindow_.top() + row;
            if (row >= 0 && index < static_cast<int>(messages_.size())) {
                pane_ = Pane::Messages;
                messageCursor_ = index;
            }
        }
    }
}

void EditorShell::updateModal() {
    const FieldDesc* desc = nullptr;
    if (modalRow_.desc != nullptr) {
        desc = modalRow_.child != nullptr ? modalRow_.child : modalRow_.desc;
    }
    switch (modal_) {
        case Modal::TextEdit: {
            for (int cp = GetCharPressed(); cp != 0; cp = GetCharPressed()) {
                textEdit_.appendCodepoint(cp);
            }
            if (keyPressed(KEY_BACKSPACE)) {
                textEdit_.backspace();
            }
            if (IsKeyPressed(KEY_ENTER)) {
                const bool isId = desc != nullptr && desc->isId;
                const OrderedJson before = rowValue(modalRow_);
                const std::string oldId =
                    before.is_string() ? before.get<std::string>() : std::string();
                writeRowValue(modalRow_, OrderedJson(textEdit_.value()));
                if (isId && oldId != textEdit_.value()) {
                    const int refs = docs_.referenceCount(oldId, category(), entityMenu_.cursor());
                    if (refs > 0) {
                        status_ = "Renamed id; " + std::to_string(refs) +
                                  " reference(s) still use '" + oldId + "' - validate!";
                    }
                }
                modal_ = Modal::None;
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                modal_ = Modal::None;
            }
            break;
        }
        case Modal::Pick: {
            const bool simPick = simPickTarget_ != nullptr || simPickAppendEnemy_;
            bool filterChanged = false;
            for (int cp = GetCharPressed(); cp != 0; cp = GetCharPressed()) {
                if (cp > 32 && cp <= 126) {
                    pickFilter_.push_back(static_cast<char>(cp));
                    filterChanged = true;
                }
            }
            if (keyPressed(KEY_BACKSPACE) && !pickFilter_.empty()) {
                pickFilter_.pop_back();
                filterChanged = true;
            }
            if (filterChanged) {
                if (simPick) {
                    rebuildSimPickMenu();
                } else if (desc != nullptr) {
                    buildPickMenu(docs_, *desc, pickFilter_, {}, !desc->required, pickMenu_,
                                  pickValues_);
                }
                pickWindow_.reset();
            }
            if (keyPressed(KEY_UP)) {
                pickMenu_.moveUp();
            }
            if (keyPressed(KEY_DOWN)) {
                pickMenu_.moveDown();
            }
            if (IsKeyPressed(KEY_ENTER) && pickMenu_.currentEnabled()) {
                if (simPick) {
                    const std::string& value =
                        pickValues_[static_cast<std::size_t>(pickMenu_.cursor())];
                    if (simPickAppendEnemy_) {
                        if (!value.empty()) {
                            simConfig_.enemyIds.push_back(value);
                        }
                    } else if (simPickTarget_ != nullptr) {
                        *simPickTarget_ = value;
                    }
                    simPickTarget_ = nullptr;
                    simPickAppendEnemy_ = false;
                    modal_ = Modal::None;
                } else if (desc != nullptr) {
                    const std::string& value =
                        pickValues_[static_cast<std::size_t>(pickMenu_.cursor())];
                    if (value.empty() && desc->kind == FieldKind::Enum) {
                        writeRowValue(modalRow_, OrderedJson(desc->defaultString));
                    } else {
                        writeRowValue(modalRow_, OrderedJson(value));
                    }
                    modal_ = Modal::None;
                }
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                simPickTarget_ = nullptr;
                simPickAppendEnemy_ = false;
                modal_ = Modal::None;
            }
            break;
        }
        case Modal::ListEdit: {
            if (desc == nullptr) {
                modal_ = Modal::None;
                break;
            }
            OrderedJson list = rowValue(modalRow_);
            if (!list.is_array()) {
                list = OrderedJson::array();
            }
            if (listPicking_) {
                bool filterChanged = false;
                for (int cp = GetCharPressed(); cp != 0; cp = GetCharPressed()) {
                    if (cp > 32 && cp <= 126) {
                        pickFilter_.push_back(static_cast<char>(cp));
                        filterChanged = true;
                    }
                }
                if (keyPressed(KEY_BACKSPACE) && !pickFilter_.empty()) {
                    pickFilter_.pop_back();
                    filterChanged = true;
                }
                if (filterChanged) {
                    std::vector<std::string> exclude;
                    for (const OrderedJson& el : list) {
                        if (el.is_string()) {
                            exclude.push_back(el.get<std::string>());
                        }
                    }
                    buildPickMenu(docs_, *desc, pickFilter_, exclude, false, pickMenu_,
                                  pickValues_);
                    pickWindow_.reset();
                }
                if (keyPressed(KEY_UP)) {
                    pickMenu_.moveUp();
                }
                if (keyPressed(KEY_DOWN)) {
                    pickMenu_.moveDown();
                }
                if (IsKeyPressed(KEY_ENTER) && pickMenu_.currentEnabled()) {
                    list.push_back(pickValues_[static_cast<std::size_t>(pickMenu_.cursor())]);
                    writeRowValue(modalRow_, list);
                    listPicking_ = false;
                    openListEdit(modalRow_);
                    listMenu_.setCursor(static_cast<int>(list.size()) - 1);
                    modal_ = Modal::ListEdit;
                }
                if (IsKeyPressed(KEY_ESCAPE)) {
                    listPicking_ = false;
                }
                break;
            }
            if (keyPressed(KEY_UP)) {
                if (ctrlDown() && listMenu_.cursor() > 0 &&
                    listMenu_.cursor() < static_cast<int>(list.size())) {
                    std::swap(list[static_cast<std::size_t>(listMenu_.cursor())],
                              list[static_cast<std::size_t>(listMenu_.cursor()) - 1]);
                    writeRowValue(modalRow_, list);
                    const int cursor = listMenu_.cursor() - 1;
                    openListEdit(modalRow_);
                    listMenu_.setCursor(cursor);
                } else {
                    listMenu_.moveUp();
                }
            }
            if (keyPressed(KEY_DOWN)) {
                if (ctrlDown() && listMenu_.cursor() + 1 < static_cast<int>(list.size())) {
                    std::swap(list[static_cast<std::size_t>(listMenu_.cursor())],
                              list[static_cast<std::size_t>(listMenu_.cursor()) + 1]);
                    writeRowValue(modalRow_, list);
                    const int cursor = listMenu_.cursor() + 1;
                    openListEdit(modalRow_);
                    listMenu_.setCursor(cursor);
                } else {
                    listMenu_.moveDown();
                }
            }
            if (IsKeyPressed(KEY_DELETE) && listMenu_.cursor() < static_cast<int>(list.size()) &&
                !list.empty()) {
                list.erase(list.begin() + listMenu_.cursor());
                writeRowValue(modalRow_, list);
                const int cursor = listMenu_.cursor();
                openListEdit(modalRow_);
                listMenu_.setCursor(cursor);
            }
            if (IsKeyPressed(KEY_N)) {
                std::vector<std::string> exclude;
                for (const OrderedJson& el : list) {
                    if (el.is_string()) {
                        exclude.push_back(el.get<std::string>());
                    }
                }
                pickFilter_.clear();
                buildPickMenu(docs_, *desc, pickFilter_, exclude, false, pickMenu_, pickValues_);
                pickWindow_.reset();
                listPicking_ = true;
            }
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) {
                modal_ = Modal::None;
            }
            break;
        }
        case Modal::ArrayEdit: {
            if (modalRow_.desc == nullptr) {
                modal_ = Modal::None;
                break;
            }
            OrderedJson list = rowValue(modalRow_);
            if (!list.is_array()) {
                list = OrderedJson::array();
            }
            const std::vector<FieldDesc>& children = modalRow_.desc->children;
            if (arrayEditingEntry_) {
                const int entry = arrayMenu_.cursor();
                if (entry >= static_cast<int>(list.size())) {
                    arrayEditingEntry_ = false;
                    break;
                }
                OrderedJson& obj = list[static_cast<std::size_t>(entry)];
                if (keyPressed(KEY_UP) && arrayFieldCursor_ > 0) {
                    --arrayFieldCursor_;
                }
                if (keyPressed(KEY_DOWN) &&
                    arrayFieldCursor_ + 1 < static_cast<int>(children.size())) {
                    ++arrayFieldCursor_;
                }
                const int dir = keyPressed(KEY_RIGHT) ? 1 : (keyPressed(KEY_LEFT) ? -1 : 0);
                if (dir != 0) {
                    const FieldDesc& child =
                        children[static_cast<std::size_t>(arrayFieldCursor_)];
                    OrderedJson v = fieldValue(obj, child);
                    if (child.kind == FieldKind::Int || child.kind == FieldKind::Float) {
                        v = steppedValue(child, v, dir);
                    } else if (child.kind == FieldKind::Enum || child.kind == FieldKind::IdRef) {
                        std::vector<std::string> options;
                        for (const auto& [value, suffix] : pickCandidates(docs_, child)) {
                            options.push_back(value);
                        }
                        if (!options.empty()) {
                            const std::string current =
                                v.is_string() ? v.get<std::string>() : "";
                            int index = 0;
                            for (std::size_t i = 0; i < options.size(); ++i) {
                                if (options[i] == current) {
                                    index = static_cast<int>(i);
                                    break;
                                }
                            }
                            const int n = static_cast<int>(options.size());
                            index = (index + dir + n) % n;
                            v = OrderedJson(options[static_cast<std::size_t>(index)]);
                        }
                    } else if (child.kind == FieldKind::Bool) {
                        v = OrderedJson(!(v.is_boolean() && v.get<bool>()));
                    }
                    obj[child.key] = v;
                    writeRowValue(modalRow_, list);
                }
                if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) {
                    arrayEditingEntry_ = false;
                    const int cursor = arrayMenu_.cursor();
                    openArrayEdit(modalRow_);
                    arrayMenu_.setCursor(cursor);
                }
                break;
            }
            if (keyPressed(KEY_UP)) {
                arrayMenu_.moveUp();
            }
            if (keyPressed(KEY_DOWN)) {
                arrayMenu_.moveDown();
            }
            if (IsKeyPressed(KEY_N)) {
                OrderedJson entry = OrderedJson::object();
                for (const FieldDesc& child : children) {
                    if (child.kind == FieldKind::IdRef || child.kind == FieldKind::Enum) {
                        const auto candidates = pickCandidates(docs_, child);
                        entry[child.key] = candidates.empty()
                                               ? child.defaultString
                                               : candidates.front().first;
                    } else {
                        entry[child.key] = fieldValue(entry, child);
                    }
                }
                list.push_back(entry);
                writeRowValue(modalRow_, list);
                openArrayEdit(modalRow_);
                arrayMenu_.setCursor(static_cast<int>(list.size()) - 1);
            }
            if (IsKeyPressed(KEY_DELETE) && !list.empty() &&
                arrayMenu_.cursor() < static_cast<int>(list.size())) {
                list.erase(list.begin() + arrayMenu_.cursor());
                writeRowValue(modalRow_, list);
                const int cursor = arrayMenu_.cursor();
                openArrayEdit(modalRow_);
                arrayMenu_.setCursor(cursor);
            }
            if (IsKeyPressed(KEY_ENTER) && !list.empty() &&
                arrayMenu_.cursor() < static_cast<int>(list.size())) {
                arrayEditingEntry_ = true;
                arrayFieldCursor_ = 0;
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                modal_ = Modal::None;
            }
            break;
        }
        case Modal::ConfirmDelete: {
            if (IsKeyPressed(KEY_ENTER)) {
                if (docs_.removeEntity(category(), entityMenu_.cursor())) {
                    rebuildEntities(true);
                    rebuildForm();
                    status_ = "Deleted.";
                }
                modal_ = Modal::None;
            }
            if (IsKeyPressed(KEY_ESCAPE)) {
                modal_ = Modal::None;
            }
            break;
        }
        case Modal::ConfirmQuit: {
            if (IsKeyPressed(KEY_ENTER)) {
                saveAll();
                if (!docs_.anyDirty()) {
                    quit_ = true;
                }
                modal_ = Modal::None;
            }
            if (IsKeyPressed(KEY_D)) {
                quit_ = true;
            }
            if (IsKeyPressed(KEY_ESCAPE) && quitCancellable_) {
                modal_ = Modal::None;
            }
            break;
        }
        case Modal::None:
            break;
    }
}

// --- rendering --------------------------------------------------------------

void EditorShell::render() {
    const ui::style::Palette& pal = ui::style::palette();
    ClearBackground(pal.canvas);
    ui::drawHeaderBand("CrystalForge", kLogicalW, pal.crystal);
    ui::drawTextRight(status_.empty() ? docs_.dataDir().string() : status_, kLogicalW - 8, 8, 8,
                      status_.empty() ? pal.textHint : pal.gold);

    if (entitiesStale_) {
        rebuildEntities(true);
        // Dirty stars in the sidebar track the same edits. The two surface
        // rows must be rebuilt too, or the refresh truncates the sidebar
        // (M86 fix — this list must mirror selectCategory's).
        std::vector<ui::MenuItem> items;
        for (const CategoryInfo& info : categories()) {
            items.push_back({info.title, true, docs_.file(info.category).dirty ? "*" : ""});
        }
        items.push_back({"Sim Lab", true, ""});
        items.push_back({"Test Runner", true, ""});
        const int cursor = categoryMenu_.cursor();
        categoryMenu_.setItems(std::move(items));
        categoryMenu_.setCursor(cursor);
    }

    // Category sidebar.
    ui::drawFrame(kSideX, kTop, kSideW, kPaneH, ui::FrameStyle::Inset);
    ui::drawMenu(categoryMenu_, kSideX + 10, kTop + 8, kRowH, 11, pal.text, pal.disabled,
                 pal.cursor);
    if (pane_ == Pane::Categories) {
        ui::drawFocusBrackets(kSideX, kTop, kSideW, kPaneH, pal.cursor);
    }

    if (screen_ == Screen::SimLab) {
        renderSimLab();
        if (modal_ != Modal::None) {
            renderModal();
        }
        return;
    }
    if (screen_ == Screen::Tests) {
        renderTests();
        if (modal_ != Modal::None) {
            renderModal();
        }
        return;
    }

    // Entity list.
    ui::drawFrame(kEntX, kTop, kEntW, kPaneH, ui::FrameStyle::Inset);
    entityWindow_.follow(static_cast<int>(entityMenu_.size()), kEntityRows, entityMenu_.cursor());
    ui::drawMenuScrolled(entityMenu_, entityWindow_, kEntityRows, kEntX + 10, kTop + 8, kRowH, 10,
                         kEntW - 24, pal.text, pal.disabled, pal.cursor, "editor.entities", 8);
    if (pane_ == Pane::Entities) {
        ui::drawFocusBrackets(kEntX, kTop, kEntW, kPaneH, pal.cursor);
    }

    // Field form.
    ui::drawFrame(kFormX, kTop, kFormW, kPaneH, ui::FrameStyle::Standard);
    formWindow_.follow(static_cast<int>(formRows_.size()), kFormRowsVisible, formCursor_);
    const int visible = formWindow_.visibleCount(static_cast<int>(formRows_.size()),
                                                 kFormRowsVisible);
    for (int i = 0; i < visible; ++i) {
        const int index = formWindow_.top() + i;
        const FormRow& row = formRows_[static_cast<std::size_t>(index)];
        const int y = kTop + 8 + i * kFormRowH;
        if (row.kind == FormRow::Kind::Section) {
            ui::drawSectionHeader(row.label, kFormX + 8, y + 1, kFormW - 20);
            continue;
        }
        const bool focused = pane_ == Pane::Form && index == formCursor_;
        if (focused) {
            ui::drawSelectionSlab(kFormX + 4, y - 1, kFormW - 10, kFormRowH);
            ui::drawChevron(kFormX + 6, y + 2, pal.cursor, ui::motionPhase());
        }
        const Color labelColor = row.kind == FormRow::Kind::Unknown
                                     ? pal.disabled
                                     : (focused ? pal.cursor : pal.text);
        ui::drawTextFitted(row.label, kFormX + 14, y + 1, kFormValueX - 18, 10, labelColor,
                           "editor.form.label");
        ui::drawTextFitted(rowValueText(row), kFormX + kFormValueX, y + 1,
                           kFormW - kFormValueX - 10, 10,
                           row.kind == FormRow::Kind::Unknown ? pal.textHint : pal.textDim,
                           "editor.form.value");
    }
    if (pane_ == Pane::Form) {
        ui::drawFocusBrackets(kFormX, kTop, kFormW, kPaneH, pal.cursor);
    }

    // Messages / validation.
    ui::drawFrame(kMsgX, kTop, kMsgW, kPaneH, ui::FrameStyle::Inset);
    const bool dbOk = validation_ && validation_->databaseOk;
    ui::drawTextFitted(validation_ ? (dbOk ? "Content OK" : "Content errors") : "Not validated",
                       kMsgX + 8, kTop + 6, kMsgW - 16, 10, dbOk ? pal.success : pal.dangerText,
                       "editor.msg.header");
    messageWindow_.follow(static_cast<int>(messages_.size()), kMsgRows, messageCursor_);
    const int msgVisible =
        messageWindow_.visibleCount(static_cast<int>(messages_.size()), kMsgRows);
    for (int i = 0; i < msgVisible; ++i) {
        const int index = messageWindow_.top() + i;
        const MessageRow& row = messages_[static_cast<std::size_t>(index)];
        const int y = kTop + 20 + i * kMsgRowH;
        const bool focused = pane_ == Pane::Messages && index == messageCursor_;
        if (focused) {
            ui::drawSelectionSlab(kMsgX + 4, y - 1, kMsgW - 10, kMsgRowH);
        }
        ui::drawTextFitted(row.text, kMsgX + 8, y, kMsgW - 16, 8,
                           row.isError ? pal.dangerText : pal.textDim, "editor.msg.row");
    }
    if (pane_ == Pane::Messages) {
        ui::drawFocusBrackets(kMsgX, kTop, kMsgW, kPaneH, pal.cursor);
    }

    // Footer hints.
    std::vector<ui::Hint> hints;
    switch (pane_) {
        case Pane::Categories:
            hints = {{"Up/Dn", "Category"}, {"Enter", "Entities"}, {"Ctrl+S", "Save"},
                     {"F5", "Validate"}, {"Esc", "Quit"}};
            break;
        case Pane::Entities:
            hints = {{"Enter", "Edit"}, {"N", "New"}, {"D", "Duplicate"}, {"Del", "Delete"},
                     {"Ctrl+S", "Save"}};
            break;
        case Pane::Form:
            hints = {{"Lf/Rt", "Step"}, {"Enter", "Edit"}, {"Tab", "Pane"}, {"Ctrl+S", "Save"},
                     {"Esc", "Back"}};
            break;
        case Pane::Messages:
            hints = {{"Up/Dn", "Select"}, {"Enter", "Jump to"}, {"F5", "Re-validate"},
                     {"Tab", "Pane"}};
            break;
    }
    ui::drawFooterHints(hints, kLogicalW, kLogicalH, "editor.footer");

    if (modal_ != Modal::None) {
        renderModal();
    }
}

void EditorShell::renderModal() {
    const ui::style::Palette& pal = ui::style::palette();
    ui::drawModalDim(kLogicalW, kLogicalH);
    const FieldDesc* desc = nullptr;
    if (modalRow_.desc != nullptr) {
        desc = modalRow_.child != nullptr ? modalRow_.child : modalRow_.desc;
    }
    switch (modal_) {
        case Modal::TextEdit: {
            // Owner fix 2026-08-17: bodies run long, so the modal is a tall
            // window onto the TAIL of the wrapped text (the caret always
            // stays visible while typing), with a character counter and a
            // "lines above" marker instead of clipping the head silently.
            constexpr int kEditLines = 10;
            const int step = ui::lineHeight(10);
            const int w = 480, h = 34 + kEditLines * step + 14;
            const int x = (kLogicalW - w) / 2, y = (kLogicalH - h) / 2;
            ui::drawFrame(x, y, w, h, ui::FrameStyle::Raised);
            ui::drawTextFitted((desc != nullptr ? desc->label : std::string("Text")) + ":", x + 12,
                               y + 8, w - 120, 10, pal.gold, "editor.textedit.label");
            ui::drawTextRight(TextFormat("%d/%d", static_cast<int>(textEdit_.value().size()),
                                         static_cast<int>(textEdit_.maxLength())),
                              x + w - 12, y + 8, 8, pal.textHint);
            const std::vector<std::string> lines =
                ui::wrapText(textEdit_.value() + "_", w - 24, 10, ui::raylibMeasure());
            const int first = std::max(0, static_cast<int>(lines.size()) - kEditLines);
            if (first > 0) {
                ui::drawTextRight(TextFormat("^ %d more line%s above", first,
                                             first == 1 ? "" : "s"),
                                  x + w - 12, y + 8 + 9, 8, pal.textHint);
            }
            for (int i = first; i < static_cast<int>(lines.size()); ++i) {
                ui::drawText(lines[static_cast<std::size_t>(i)], x + 12,
                             y + 24 + (i - first) * step, 10, pal.text);
            }
            ui::drawFooterHints({{"Enter", "Apply"}, {"Esc", "Cancel"}}, kLogicalW, kLogicalH,
                                "editor.textedit.hints");
            break;
        }
        case Modal::Pick:
        case Modal::ListEdit: {
            const bool picking = modal_ == Modal::Pick || listPicking_;
            const int w = 300, h = 240;
            const int x = (kLogicalW - w) / 2, y = (kLogicalH - h) / 2;
            ui::drawFrame(x, y, w, h, ui::FrameStyle::Raised);
            const bool simPick = simPickTarget_ != nullptr || simPickAppendEnemy_;
            std::string title = simPick ? confirmText_
                                        : (desc != nullptr ? desc->label : std::string("Values"));
            if (picking) {
                title += pickFilter_.empty() ? "  (type to filter)" : "  filter: " + pickFilter_;
            }
            ui::drawTextFitted(title, x + 12, y + 8, w - 24, 10, pal.gold, "editor.pick.title");
            ui::Menu& menu = picking ? pickMenu_ : listMenu_;
            ui::ScrollWindow& window = picking ? pickWindow_ : listWindow_;
            const int rows = 14;
            window.follow(static_cast<int>(menu.size()), rows, menu.cursor());
            ui::drawMenuScrolled(menu, window, rows, x + 14, y + 24, kRowH, 10, w - 32, pal.text,
                                 pal.disabled, pal.cursor, "editor.pick.rows", 8);
            ui::drawFooterHints(picking
                                    ? std::vector<ui::Hint>{{"Enter", "Choose"}, {"Esc", "Back"}}
                                    : std::vector<ui::Hint>{{"N", "Add"}, {"Del", "Remove"},
                                                            {"Ctrl+Up/Dn", "Reorder"},
                                                            {"Esc", "Done"}},
                                kLogicalW, kLogicalH, "editor.pick.hints");
            break;
        }
        case Modal::ArrayEdit: {
            const int w = 360, h = 260;
            const int x = (kLogicalW - w) / 2, y = (kLogicalH - h) / 2;
            ui::drawFrame(x, y, w, h, ui::FrameStyle::Raised);
            ui::drawTextFitted(modalRow_.desc != nullptr ? modalRow_.desc->label : "Entries",
                               x + 12, y + 8, w - 24, 10, pal.gold, "editor.array.title");
            const int rows = 9;
            arrayWindow_.follow(static_cast<int>(arrayMenu_.size()), rows, arrayMenu_.cursor());
            ui::drawMenuScrolled(arrayMenu_, arrayWindow_, rows, x + 14, y + 24, kRowH, 10, w - 32,
                                 pal.text, pal.disabled, pal.cursor, "editor.array.rows");
            if (arrayEditingEntry_ && modalRow_.desc != nullptr) {
                const OrderedJson list = rowValue(modalRow_);
                const int entry = arrayMenu_.cursor();
                if (list.is_array() && entry < static_cast<int>(list.size())) {
                    const OrderedJson& obj = list[static_cast<std::size_t>(entry)];
                    const std::vector<FieldDesc>& children = modalRow_.desc->children;
                    const int fieldsY = y + 24 + rows * kRowH + 6;
                    ui::drawDivider(x + 12, fieldsY - 4, w - 24);
                    for (std::size_t i = 0; i < children.size(); ++i) {
                        const int fy = fieldsY + static_cast<int>(i) * kFormRowH;
                        const bool focused = static_cast<int>(i) == arrayFieldCursor_;
                        if (focused) {
                            ui::drawSelectionSlab(x + 10, fy - 1, w - 20, kFormRowH);
                        }
                        const OrderedJson v = fieldValue(obj, children[i]);
                        ui::drawTextFitted(children[i].label, x + 16, fy, 120, 10,
                                           focused ? pal.cursor : pal.text, "editor.array.flabel");
                        ui::drawTextFitted(v.is_string() ? v.get<std::string>() : v.dump(),
                                           x + 140, fy, w - 152, 10, pal.textDim,
                                           "editor.array.fvalue");
                    }
                }
                ui::drawFooterHints({{"Lf/Rt", "Change"}, {"Up/Dn", "Field"}, {"Enter", "Done"}},
                                    kLogicalW, kLogicalH, "editor.array.hints");
            } else {
                ui::drawFooterHints({{"Enter", "Edit entry"}, {"N", "Add"}, {"Del", "Remove"},
                                     {"Esc", "Done"}},
                                    kLogicalW, kLogicalH, "editor.array.hints");
            }
            break;
        }
        case Modal::ConfirmDelete:
        case Modal::ConfirmQuit: {
            const int w = 380, h = 84;
            const int x = (kLogicalW - w) / 2, y = (kLogicalH - h) / 2;
            ui::drawFrame(x, y, w, h, ui::FrameStyle::Danger);
            ui::drawTextWrapped(confirmText_, x + 14, y + 10, w - 28, 10, pal.text,
                                "editor.confirm.text", 3);
            if (modal_ == Modal::ConfirmDelete) {
                ui::drawFooterHints({{"Enter", "Delete"}, {"Esc", "Cancel"}}, kLogicalW, kLogicalH,
                                    "editor.confirm.hints");
            } else if (quitCancellable_) {
                ui::drawFooterHints({{"Enter", "Save & quit"}, {"D", "Discard & quit"},
                                     {"Esc", "Cancel"}},
                                    kLogicalW, kLogicalH, "editor.confirm.hints");
            } else {
                ui::drawFooterHints({{"Enter", "Save & quit"}, {"D", "Discard & quit"}},
                                    kLogicalW, kLogicalH, "editor.confirm.hints");
            }
            break;
        }
        case Modal::None:
            break;
    }
}

// --- M60 sim lab -------------------------------------------------------------

namespace {

const char* modeName(OpponentMode mode) {
    switch (mode) {
        case OpponentMode::Manual: return "custom team";
        case OpponentMode::Boss: return "boss + court";
        case OpponentMode::BossRush: return "boss rush";
        case OpponentMode::King: return "the King";
        case OpponentMode::Endless: return "endless wave";
    }
    return "?";
}

const char* tierLabel(GearTier tier) {
    switch (tier) {
        case GearTier::None: return "bare";
        case GearTier::Median: return "median gear";
        case GearTier::Best: return "best gear";
    }
    return "?";
}

std::string orDash(const std::string& s) { return s.empty() ? "-" : s; }

}  // namespace

std::vector<EditorShell::SimRow> EditorShell::buildSimRows() const {
    std::vector<SimRow> rows;
    auto add = [&](SimRow::Kind kind, std::string label, std::string value, int member = 0,
                   int slot = 0) {
        SimRow row;
        row.kind = kind;
        row.member = member;
        row.slot = slot;
        row.label = std::move(label);
        row.value = std::move(value);
        rows.push_back(std::move(row));
    };
    add(SimRow::Kind::Level, "Level", std::to_string(simConfig_.level));
    add(SimRow::Kind::Preset, "Apply preset (Enter)", tierLabel(simPreset_));
    for (int m = 0; m < static_cast<int>(simConfig_.members.size()); ++m) {
        const SimMemberSpec& spec = simConfig_.members[static_cast<std::size_t>(m)];
        add(SimRow::Kind::MemberClass, "M" + std::to_string(m + 1) + " class",
            orDash(spec.classId), m);
        if (spec.classId.empty()) {
            continue;
        }
        add(SimRow::Kind::MemberGear, "  weapon", orDash(spec.weapon), m, 0);
        add(SimRow::Kind::MemberGear, "  armor", orDash(spec.armor), m, 1);
        add(SimRow::Kind::MemberGear, "  accessory", orDash(spec.accessory), m, 2);
        add(SimRow::Kind::MemberPassive, "  passive", orDash(spec.passive), m);
    }
    add(SimRow::Kind::Mode, "Opponent", modeName(simConfig_.mode));
    switch (simConfig_.mode) {
        case OpponentMode::Manual: {
            for (int i = 0; i < static_cast<int>(simConfig_.enemyIds.size()); ++i) {
                add(SimRow::Kind::RemoveEnemy, "  enemy (Del)",
                    simConfig_.enemyIds[static_cast<std::size_t>(i)], i);
            }
            add(SimRow::Kind::AddEnemy, "  add enemy", ">");
            add(SimRow::Kind::Scale, "Stat scale %", std::to_string(simConfig_.statScalePct));
            break;
        }
        case OpponentMode::Boss:
            add(SimRow::Kind::Boss, "  boss", orDash(simConfig_.bossId));
            add(SimRow::Kind::Scale, "Stat scale %", std::to_string(simConfig_.statScalePct));
            break;
        case OpponentMode::BossRush:
            add(SimRow::Kind::RushIndex, "  rush fight #",
                std::to_string(simConfig_.rushIndex + 1));
            break;
        case OpponentMode::King:
            break;
        case OpponentMode::Endless:
            add(SimRow::Kind::Wave, "  wave", std::to_string(simConfig_.endlessWave));
            break;
    }
    add(SimRow::Kind::Seeds, "Seeds", std::to_string(simConfig_.seeds));
    add(SimRow::Kind::Run, "RUN SWEEP", "Enter");
    if (simResult_ && simResult_->ok) {
        add(SimRow::Kind::ExportMd, "Export report (.md)", "Enter");
        add(SimRow::Kind::ExportCsv, "Export report (.csv)", "Enter");
    }
    return rows;
}

void EditorShell::applySimPreset() {
    content::ContentDatabase db;
    content::LoadReport rep;
    buildDatabase(docs_, db, rep);
    simConfig_.members.clear();
    for (const content::ClassDef* cls : defaultSimClasses(db)) {
        simConfig_.members.push_back(presetMember(db, cls->id, simPreset_));
    }
    simConfig_.members.resize(4);
}

void EditorShell::stepSimRow(const SimRow& row, int direction) {
    auto cycleId = [&](std::string& target, Category category, bool allowEmpty) {
        std::vector<std::string> ids;
        if (allowEmpty) {
            ids.emplace_back();
        }
        const int count = docs_.entityCount(category);
        std::vector<std::string> sorted;
        for (int i = 0; i < count; ++i) {
            sorted.push_back(docs_.entityLabel(category, i));
        }
        std::sort(sorted.begin(), sorted.end());
        ids.insert(ids.end(), sorted.begin(), sorted.end());
        if (ids.empty()) {
            return;
        }
        int index = 0;
        for (std::size_t i = 0; i < ids.size(); ++i) {
            if (ids[i] == target) {
                index = static_cast<int>(i);
                break;
            }
        }
        const int n = static_cast<int>(ids.size());
        target = ids[static_cast<std::size_t>((index + direction + n) % n)];
    };

    switch (row.kind) {
        case SimRow::Kind::Level:
            simConfig_.level = std::clamp(simConfig_.level + direction, 1, kMaxLevel);
            break;
        case SimRow::Kind::Preset: {
            const int t = (static_cast<int>(simPreset_) + direction + 3) % 3;
            simPreset_ = static_cast<GearTier>(t);
            break;
        }
        case SimRow::Kind::MemberClass:
            cycleId(simConfig_.members[static_cast<std::size_t>(row.member)].classId,
                    Category::Classes, true);
            break;
        case SimRow::Kind::MemberPassive:
            cycleId(simConfig_.members[static_cast<std::size_t>(row.member)].passive,
                    Category::Passives, true);
            break;
        case SimRow::Kind::Mode: {
            const int m = (static_cast<int>(simConfig_.mode) + direction + 5) % 5;
            simConfig_.mode = static_cast<OpponentMode>(m);
            break;
        }
        case SimRow::Kind::RushIndex:
            simConfig_.rushIndex = std::clamp(simConfig_.rushIndex + direction, 0, 15);
            break;
        case SimRow::Kind::Wave:
            simConfig_.endlessWave = std::clamp(simConfig_.endlessWave + direction, 1, 99);
            break;
        case SimRow::Kind::Scale:
            simConfig_.statScalePct =
                std::clamp(simConfig_.statScalePct + direction * 10, 50, 600);
            break;
        case SimRow::Kind::Seeds: {
            const int options[] = {10, 100, 1000};
            int index = 1;
            for (int i = 0; i < 3; ++i) {
                if (options[i] == simConfig_.seeds) {
                    index = i;
                }
            }
            simConfig_.seeds = options[(index + direction + 3) % 3];
            break;
        }
        case SimRow::Kind::MemberGear:
        case SimRow::Kind::Boss:
        case SimRow::Kind::AddEnemy:
        case SimRow::Kind::RemoveEnemy:
        case SimRow::Kind::Run:
        case SimRow::Kind::ExportMd:
        case SimRow::Kind::ExportCsv:
            break;  // Enter-driven rows
    }
}

void EditorShell::activateSimRow(const SimRow& row) {
    SimMemberSpec* spec =
        row.member < static_cast<int>(simConfig_.members.size())
            ? &simConfig_.members[static_cast<std::size_t>(row.member)]
            : nullptr;
    switch (row.kind) {
        case SimRow::Kind::Preset:
            applySimPreset();
            status_ = std::string("Preset applied: ") + tierLabel(simPreset_) + ".";
            break;
        case SimRow::Kind::MemberClass:
            if (spec != nullptr) {
                openSimPick(&spec->classId, Category::Classes, "", false);
            }
            break;
        case SimRow::Kind::MemberGear:
            if (spec != nullptr) {
                static const char* kSlots[] = {"weapon", "armor", "accessory"};
                std::string* target = row.slot == 0   ? &spec->weapon
                                      : row.slot == 1 ? &spec->armor
                                                      : &spec->accessory;
                openSimPick(target, Category::Items, kSlots[row.slot], false);
            }
            break;
        case SimRow::Kind::MemberPassive:
            if (spec != nullptr) {
                openSimPick(&spec->passive, Category::Passives, "", false);
            }
            break;
        case SimRow::Kind::Boss:
            openSimPick(&simConfig_.bossId, Category::Bosses, "", false);
            break;
        case SimRow::Kind::AddEnemy:
            openSimPick(nullptr, Category::Enemies, "", true);
            break;
        case SimRow::Kind::RemoveEnemy:
            break;  // Del removes (handled in updateSimLab)
        case SimRow::Kind::Run:
            runSimSweep();
            break;
        case SimRow::Kind::ExportMd:
            exportSimReport(true);
            break;
        case SimRow::Kind::ExportCsv:
            exportSimReport(false);
            break;
        default:
            break;
    }
}

void EditorShell::openSimPick(std::string* target, Category category,
                              const std::string& slotFilter, bool appendEnemy) {
    simPickTarget_ = target;
    simPickAppendEnemy_ = appendEnemy;
    simPickCategory_ = category;
    simPickSlotFilter_ = slotFilter;
    modalRow_ = FormRow{};  // no document row backs a sim pick
    confirmText_ = appendEnemy ? "Add enemy" : infoFor(category).title;
    pickFilter_.clear();
    rebuildSimPickMenu();
    pickWindow_.reset();
    modal_ = Modal::Pick;
}

void EditorShell::rebuildSimPickMenu() {
    pickValues_.clear();
    std::vector<ui::MenuItem> items;
    if (!simPickAppendEnemy_) {
        pickValues_.emplace_back();
        items.push_back({"(clear)", true, ""});
    }
    std::vector<std::pair<std::string, std::string>> candidates;
    const int count = docs_.entityCount(simPickCategory_);
    for (int i = 0; i < count; ++i) {
        if (!simPickSlotFilter_.empty()) {
            // Items pane: only equipment/relics of the requested slot.
            const OrderedJson* entity = docs_.entityAt(simPickCategory_, i);
            if (entity == nullptr) {
                continue;
            }
            const auto slot = entity->find("slot");
            if (slot == entity->end() || !slot->is_string() ||
                slot->get<std::string>() != simPickSlotFilter_) {
                continue;
            }
        }
        candidates.emplace_back(docs_.entityLabel(simPickCategory_, i),
                                docs_.entitySuffix(simPickCategory_, i));
    }
    std::sort(candidates.begin(), candidates.end());
    for (const auto& [value, suffix] : candidates) {
        if (!pickFilter_.empty() && value.find(pickFilter_) == std::string::npos) {
            continue;
        }
        pickValues_.push_back(value);
        items.push_back({value, true, suffix});
    }
    if (items.empty()) {
        items.push_back({"(no match)", false, ""});
    }
    pickMenu_.setItems(std::move(items));
}

void EditorShell::runSimSweep() {
    content::ContentDatabase db;
    content::LoadReport rep;
    if (!buildDatabase(docs_, db, rep)) {
        status_ = "Fix content errors before simulating (" +
                  std::to_string(rep.errorCount()) + ").";
        return;
    }
    SimLabResult result = runSweep(simConfig_, db);
    if (!result.ok) {
        status_ = "Sweep failed: " + result.error;
        return;
    }
    simPrevious_ = std::move(simResult_);
    simResult_ = std::move(result);
    status_ = "Sweep done: " + std::to_string(simResult_->runs) + " seeds.";
}

void EditorShell::exportSimReport(bool markdown) {
    if (!simResult_ || !simResult_->ok) {
        return;
    }
    const std::filesystem::path dir = docs_.dataDir().parent_path() / "reports";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tm);
    const std::filesystem::path path =
        dir / (std::string("sim_") + stamp + (markdown ? ".md" : ".csv"));
    const std::string text =
        markdown ? reportMarkdown(simConfig_, *simResult_,
                                  simPrevious_ && simPrevious_->ok ? &*simPrevious_ : nullptr)
                 : reportCsv(simConfig_, *simResult_);
    std::string error;
    status_ = platform::writeTextFileAtomically(path, text, error)
                  ? "Report written: " + path.filename().string()
                  : "Export failed: " + error;
}

void EditorShell::updateSimLab() {
    const std::vector<SimRow> rows = buildSimRows();
    const int count = static_cast<int>(rows.size());
    simCursor_ = std::clamp(simCursor_, 0, count - 1);
    if (keyPressed(KEY_UP) && simCursor_ > 0) {
        --simCursor_;
    }
    if (keyPressed(KEY_DOWN) && simCursor_ + 1 < count) {
        ++simCursor_;
    }
    const SimRow& row = rows[static_cast<std::size_t>(simCursor_)];
    if (keyPressed(KEY_LEFT)) {
        stepSimRow(row, -1);
    }
    if (keyPressed(KEY_RIGHT)) {
        stepSimRow(row, 1);
    }
    if (IsKeyPressed(KEY_ENTER)) {
        activateSimRow(row);
    }
    if (IsKeyPressed(KEY_DELETE) && row.kind == SimRow::Kind::RemoveEnemy &&
        row.member < static_cast<int>(simConfig_.enemyIds.size())) {
        simConfig_.enemyIds.erase(simConfig_.enemyIds.begin() + row.member);
    }
}

void EditorShell::renderSimLab() {
    const ui::style::Palette& pal = ui::style::palette();
    constexpr int kConfigX = kEntX, kConfigW = 250;
    constexpr int kResultX = kConfigX + kConfigW + 4, kResultW = kLogicalW - kResultX - 4;

    // Config pane.
    ui::drawFrame(kConfigX, kTop, kConfigW, kPaneH, ui::FrameStyle::Standard);
    const std::vector<SimRow> rows = buildSimRows();
    simCursor_ = std::clamp(simCursor_, 0, static_cast<int>(rows.size()) - 1);
    simWindow_.follow(static_cast<int>(rows.size()), kFormRowsVisible, simCursor_);
    const int visible = simWindow_.visibleCount(static_cast<int>(rows.size()), kFormRowsVisible);
    for (int i = 0; i < visible; ++i) {
        const int index = simWindow_.top() + i;
        const SimRow& row = rows[static_cast<std::size_t>(index)];
        const int y = kTop + 8 + i * kFormRowH;
        const bool focused = pane_ != Pane::Categories && index == simCursor_;
        if (focused) {
            ui::drawSelectionSlab(kConfigX + 4, y - 1, kConfigW - 10, kFormRowH);
            ui::drawChevron(kConfigX + 6, y + 2, pal.cursor, ui::motionPhase());
        }
        const bool action = row.kind == SimRow::Kind::Run ||
                            row.kind == SimRow::Kind::ExportMd ||
                            row.kind == SimRow::Kind::ExportCsv;
        ui::drawTextFitted(row.label, kConfigX + 14, y + 1, 118, 10,
                           action ? pal.gold : (focused ? pal.cursor : pal.text),
                           "editor.sim.label");
        ui::drawTextFitted(row.value, kConfigX + 136, y + 1, kConfigW - 146, 10, pal.textDim,
                           "editor.sim.value");
    }
    if (pane_ != Pane::Categories) {
        ui::drawFocusBrackets(kConfigX, kTop, kConfigW, kPaneH, pal.cursor);
    }

    // Results pane.
    ui::drawFrame(kResultX, kTop, kResultW, kPaneH, ui::FrameStyle::Inset);
    int y = kTop + 8;
    if (!simResult_ || !simResult_->ok) {
        ui::drawTextWrapped(
            "Configure the party and opponent, then RUN SWEEP. Results, per-skill telemetry, "
            "and deltas against the previous run land here.",
            kResultX + 10, y, kResultW - 20, 10, pal.textHint, "editor.sim.empty");
    } else {
        const SimLabResult& r = *simResult_;
        const SimLabResult* p = simPrevious_ && simPrevious_->ok ? &*simPrevious_ : nullptr;
        auto line = [&](const std::string& label, const std::string& value, Color color) {
            ui::drawTextFitted(label, kResultX + 10, y, 110, 10, pal.textDim, "editor.sim.rl");
            ui::drawTextFitted(value, kResultX + 122, y, kResultW - 132, 10, color,
                               "editor.sim.rv");
            y += 12;
        };
        auto withDelta = [&](double now, double before, bool hasPrev) {
            char buffer[48];
            if (hasPrev) {
                std::snprintf(buffer, sizeof(buffer), "%.1f  (%+.1f)", now, now - before);
            } else {
                std::snprintf(buffer, sizeof(buffer), "%.1f", now);
            }
            return std::string(buffer);
        };
        ui::drawSectionHeader("Sweep result", kResultX + 8, y, kResultW - 20);
        y += 14;
        line("win rate %", withDelta(r.winRatePct(), p ? p->winRatePct() : 0, p != nullptr),
             r.winRatePct() >= 99.5 ? pal.success : pal.text);
        line("avg rounds", withDelta(r.avgRounds, p ? p->avgRounds : 0, p != nullptr), pal.text);
        line("median rounds",
             withDelta(r.medianRounds, p ? p->medianRounds : 0, p != nullptr), pal.text);
        line("rounds min/max",
             std::to_string(r.minRounds) + " / " + std::to_string(r.maxRounds), pal.text);
        line("avg party HP %",
             withDelta(r.avgHpFraction * 100.0, p ? p->avgHpFraction * 100.0 : 0, p != nullptr),
             pal.text);
        line("party KOs", std::to_string(r.partyKos), r.partyKos > 0 ? pal.dangerText : pal.text);
        line("danger", r.dangerTier, pal.gold);
        y += 4;
        ui::drawSectionHeader("Top actions", kResultX + 8, y, kResultW - 20);
        y += 14;
        std::vector<std::pair<long, std::string>> top;
        for (const auto& [id, tally] : r.telemetry.actions()) {
            top.emplace_back(tally.damage + tally.healing, id);
        }
        std::sort(top.rbegin(), top.rend());
        int shown = 0;
        for (const auto& [total, id] : top) {
            if (shown++ == 8 || y > kBottom - 26) {
                break;
            }
            const ActionTally& tally = r.telemetry.actions().at(id);
            line(id,
                 std::to_string(tally.uses) + "x  " + std::to_string(total) + " total",
                 pal.textDim);
        }
    }
    ui::drawFooterHints({{"Lf/Rt", "Adjust"}, {"Enter", "Pick/Run"}, {"Tab", "Sidebar"},
                         {"Esc", "Back"}},
                        kLogicalW, kLogicalH, "editor.sim.footer");
}

// --- M60 test runner ---------------------------------------------------------

void EditorShell::startTestRun(int categoryIndex) {
    if (testRunner_ != nullptr && testRunner_->running()) {
        testSummary_ = "A run is already in progress (Del stops it).";
        return;
    }
    const std::vector<TestCategory>& cats = testCategories();
    if (categoryIndex < 0 || categoryIndex >= static_cast<int>(cats.size())) {
        return;
    }
    const TestCategory& category = cats[static_cast<std::size_t>(categoryIndex)];
    const std::filesystem::path exe = testBinaryPath(GetApplicationDirectory());
    if (!std::filesystem::exists(exe)) {
        testSummary_ = "crystal_tests.exe not found - build the tests first.";
        return;
    }
    testRunner_ = std::make_unique<platform::ProcessRunner>();
    testOutput_.clear();
    testOutput_.push_back("> crystal_tests " + catch2Spec(category));
    testWindow_.reset();
    std::string error;
    if (!testRunner_->start(exe, testInvocation(category), error)) {
        testSummary_ = "Launch failed: " + error;
        testRunner_.reset();
        return;
    }
    testRunningCategory_ = categoryIndex;
    testSummary_ = "Running " + category.name + "...";
}

void EditorShell::updateTests() {
    if (testRunner_ == nullptr) {
        return;
    }
    std::vector<std::string> fresh = testRunner_->drainLines();
    if (!fresh.empty()) {
        for (std::string& line : fresh) {
            testOutput_.push_back(std::move(line));
        }
        // Follow the tail as output streams in.
        testWindow_.follow(static_cast<int>(testOutput_.size()), kMsgRows,
                           static_cast<int>(testOutput_.size()) - 1);
    }
    if (testRunningCategory_ >= 0 && !testRunner_->running()) {
        const int code = testRunner_->exitCode();
        testSummary_ = code == 0 ? "PASSED" : "FAILED (exit " + std::to_string(code) + ")";
        testRunningCategory_ = -1;
    }
}

void EditorShell::renderTests() {
    const ui::style::Palette& pal = ui::style::palette();
    constexpr int kCatX = kEntX, kCatW = 150;
    constexpr int kOutX = kCatX + kCatW + 4, kOutW = kLogicalW - kOutX - 4;

    ui::drawFrame(kCatX, kTop, kCatW, kPaneH, ui::FrameStyle::Standard);
    const std::vector<TestCategory>& cats = testCategories();
    for (int i = 0; i < static_cast<int>(cats.size()); ++i) {
        const int y = kTop + 8 + i * kRowH;
        const bool focused = pane_ != Pane::Categories && i == testCursor_;
        if (focused) {
            ui::drawSelectionSlab(kCatX + 4, y - 1, kCatW - 10, kRowH);
            ui::drawChevron(kCatX + 6, y + 3, pal.cursor, ui::motionPhase());
        }
        const bool active = i == testRunningCategory_;
        ui::drawTextFitted(cats[static_cast<std::size_t>(i)].name, kCatX + 14, y + 2,
                           kCatW - 24, 10,
                           active ? pal.gold : (focused ? pal.cursor : pal.text),
                           "editor.tests.cat");
    }
    if (pane_ != Pane::Categories) {
        ui::drawFocusBrackets(kCatX, kTop, kCatW, kPaneH, pal.cursor);
    }

    ui::drawFrame(kOutX, kTop, kOutW, kPaneH, ui::FrameStyle::Inset);
    const bool passed = testSummary_ == "PASSED";
    const bool failed = testSummary_.rfind("FAILED", 0) == 0;
    ui::drawTextFitted(testSummary_.empty() ? "Enter runs the selected category." : testSummary_,
                       kOutX + 8, kTop + 6, kOutW - 16, 10,
                       passed ? pal.success : (failed ? pal.dangerText : pal.textDim),
                       "editor.tests.summary");
    const int visible = testWindow_.visibleCount(static_cast<int>(testOutput_.size()), kMsgRows);
    for (int i = 0; i < visible; ++i) {
        const int index = testWindow_.top() + i;
        ui::drawTextFitted(testOutput_[static_cast<std::size_t>(index)], kOutX + 8,
                           kTop + 20 + i * kMsgRowH, kOutW - 16, 8, pal.textDim,
                           "editor.tests.line");
    }
    ui::drawFooterHints({{"Enter", "Run"}, {"Del", "Stop"}, {"Tab", "Sidebar"}, {"Esc", "Back"}},
                        kLogicalW, kLogicalH, "editor.tests.footer");
}

}  // namespace cd::editor
