// M125 - CrystalForge's second staleness guard. The M59 sweep
// (test_editor_descriptors) only sees keys that appear in the SHIPPED data, so
// an optional loader key nobody has authored yet - inert by default, exactly
// how most fields are born - could sit in the loader with no editor field and
// no test noticing. This audit reads the loader's own source: every key
// ContentLoader.cpp reads for a category (req*/opt* readers, raw find()s, the
// keys handed to the shared read* helpers, and the helpers' own keys) must
// have a field descriptor in that category, at any depth.
//
// The function <-> file mapping is read from loadAll itself, so a new data
// file is audited the moment the loader parses it.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "core/Version.hpp"
#include "editor/CategoryDescriptors.hpp"
#include "editor/EditorTitle.hpp"

namespace {
namespace fs = std::filesystem;
using cd::editor::FieldDesc;

std::string readLoaderSource() {
    const fs::path file = fs::path(CRYSTAL_TEST_SOURCE_DIR) / "content" / "ContentLoader.cpp";
    std::ifstream in(file);
    REQUIRE(in);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// Top-level functions: a definition starts at column 0 with a return type
// (the file's own style); its body runs to the next one.
std::map<std::string, std::string> splitFunctions(const std::string& source) {
    std::map<std::string, std::string> out;
    const std::regex header(R"(^[A-Za-z_][A-Za-z0-9_:<>&\*\s]*?\b([A-Za-z_][A-Za-z0-9_]*)\()");
    std::istringstream lines(source);
    std::string line;
    std::string current;
    while (std::getline(lines, line)) {
        std::smatch m;
        const bool topLevel = !line.empty() && line[0] != ' ' && line[0] != '\t' &&
                              line[0] != '}' && line[0] != '#' && line[0] != '/' &&
                              line.rfind("namespace", 0) != 0 && line.rfind("using", 0) != 0;
        if (topLevel && std::regex_search(line, m, header)) {
            current = m[1].str();
        }
        if (!current.empty()) {
            out[current] += line;
            out[current] += '\n';
        }
    }
    return out;
}

std::set<std::string> literalKeys(const std::string& body) {
    std::set<std::string> keys;
    const std::regex patterns[] = {
        // r.reqString("id"), r.optIntMin("price", ...), r.reqEnum<Hook>("hook", ...)
        std::regex(R"re(\b(?:req|opt)[A-Za-z]*(?:<[^>]*>)?\(\s*"([A-Za-z_][A-Za-z0-9_]*)")re"),
        // el.find("learnset"), root.find("team")
        std::regex(R"re(\.find\(\s*"([A-Za-z_][A-Za-z0-9_]*)")re"),
        // readStatusList(el, source, ctx, rep, "initialStatuses", out)
        std::regex(R"re(\bread[A-Z][A-Za-z]*\([^;"]*"([A-Za-z_][A-Za-z0-9_]*)")re"),
    };
    for (const std::regex& re : patterns) {
        for (std::sregex_iterator it(body.begin(), body.end(), re), end; it != end; ++it) {
            keys.insert((*it)[1].str());
        }
    }
    return keys;
}

void collectDescriptorKeys(const std::vector<FieldDesc>& descs, std::set<std::string>& out) {
    for (const FieldDesc& d : descs) {
        out.insert(d.key);
        collectDescriptorKeys(d.children, out);
    }
}

}  // namespace

TEST_CASE("editor: every key the loader reads has a field descriptor", "[editor][m125]") {
    const std::string source = readLoaderSource();
    const std::map<std::string, std::string> functions = splitFunctions(source);
    REQUIRE(functions.count("loadAll") == 1);

    // parseX(json, "file.json", ...) inside loadAll: the loader's own map.
    std::map<std::string, std::string> parserOfFile;
    {
        const std::string& body = functions.at("loadAll");
        const std::regex call(R"re((parse[A-Za-z]+)\(\s*json\s*,\s*"([a-z_]+\.json)")re");
        for (std::sregex_iterator it(body.begin(), body.end(), call), end; it != end; ++it) {
            parserOfFile[(*it)[2].str()] = (*it)[1].str();
        }
    }

    // A parser's keys = its own literals + those of every read* helper it
    // calls (one level is the file's whole depth, but close it transitively).
    const auto keysOf = [&](const std::string& fn) {
        std::set<std::string> keys;
        std::set<std::string> seen;
        std::vector<std::string> todo{fn};
        while (!todo.empty()) {
            const std::string name = todo.back();
            todo.pop_back();
            if (!seen.insert(name).second || functions.count(name) == 0) {
                continue;
            }
            const std::string& body = functions.at(name);
            const std::set<std::string> own = literalKeys(body);
            keys.insert(own.begin(), own.end());
            for (const auto& [other, unused] : functions) {
                (void)unused;
                if (other.rfind("read", 0) == 0 && other != "readJsonFile" &&
                    body.find(other + "(") != std::string::npos) {
                    todo.push_back(other);
                }
            }
        }
        keys.erase("version");  // the root's, never an entity field
        return keys;
    };

    int audited = 0;
    for (const cd::editor::CategoryInfo& info : cd::editor::categories()) {
        INFO(info.filename);
        const auto parser = parserOfFile.find(info.filename);
        REQUIRE(parser != parserOfFile.end());  // every editor category is a loaded file
        const std::set<std::string> loaderKeys = keysOf(parser->second);
        REQUIRE_FALSE(loaderKeys.empty());

        std::set<std::string> described;
        collectDescriptorKeys(cd::editor::descriptorsFor(info.category), described);
        for (const std::string& key : loaderKeys) {
            INFO("loader key: " << key);
            CHECK(described.count(key) == 1);
        }
        ++audited;
    }
    // ...and every file the loader parses is an editor category.
    CHECK(audited == static_cast<int>(parserOfFile.size()));
}

TEST_CASE("editor: the window title names the build it came from (M125)", "[editor][m125]") {
    const std::string title = cd::editor::windowTitle();
    CHECK(title.rfind("CrystalForge", 0) == 0);
    CHECK(title.find(cd::version::kString) != std::string::npos);
    CHECK(std::string(cd::version::kString).size() > 0);
}
