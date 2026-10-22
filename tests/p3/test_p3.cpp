// Project 3 tests: registry, workspace paths, and calculator evaluation.
//
// Build & run:   cmake --build build && ./build/test_p3
// One area:      ./build/test_p3 reg_        (names starting with "reg_")
// One test:      ./build/test_p3 =reg_add_and_find

#include "../mini_test.h"
#include "../test_helpers.h"     // TempDir, write_text, args1, ...

#include "tools/calculator.h"
#include "tools/file_tools.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"

#include <cstdio>
#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// A tool whose name you choose — use it to fill the registry in tests.
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;   // an empty JSON object: {}
};

class CountedTool : public NamedTool {
public:
    CountedTool(std::string name, int& destroyed)
        : NamedTool(std::move(name)), destroyed_(destroyed) {}
    ~CountedTool() override { ++destroyed_; }
private:
    int& destroyed_;
};

// Tokenize with the provided tokenizer, then run YOUR calc_evaluate.
bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}

}  // namespace

// ---------------------------------------------------------------- examples

TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();                        // remember the address before moving
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);              // same object, not a copy
    CHECK(r.find("beta") == nullptr);
}

TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}

TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));   // sanity: accepts *something*
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}

TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}

TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
}


TEST(reg_empty) {
    ToolRegistry r;
    CHECK(r.size() == 0);
    CHECK(r.bucket_count() >= 1 && r.bucket_count() <= 64);
    CHECK(r.load_factor() == 0.0);
    CHECK(r.max_probe_length() == 0);
    CHECK(r.find("missing") == nullptr);
    int calls = 0;
    r.for_each([&](const Tool&) { ++calls; });
    CHECK(calls == 0);
    CHECK(r.size() == 0);
}

TEST(reg_null_rejected) {
    ToolRegistry r;
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 0);
    REQUIRE(r.add(std::make_unique<NamedTool>("kept")));
    Tool* original = r.find("kept");
    CHECK(!r.add(nullptr));
    CHECK(r.size() == 1);
    CHECK(r.find("kept") == original);
    CHECK(r.find("absent") == nullptr);
    CHECK(r.size() == 1);
}

TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    std::map<std::string, int> visits;
    for (int i = 0; i < 80; ++i) {
        const std::string name = "visit_" + std::to_string(i);
        visits[name] = 0;
        REQUIRE(r.add(std::make_unique<NamedTool>(name)));
    }
    r.for_each([&](const Tool& tool) {
        CHECK(visits.count(tool.name()) == 1);
        ++visits[tool.name()];
    });
    CHECK(visits.size() == 80);
    for (const auto& item : visits) CHECK(item.second == 1);
    CHECK(r.size() == 80);
}

TEST(reg_handles_collisions) {
    ToolRegistry r;
    std::vector<std::string> names;
    for (int i = 0; i < 10000 && names.size() < 3; ++i) {
        std::string name = "collision_" + std::to_string(i);
        if (ToolRegistry::hash(name) % r.bucket_count() == 0) names.push_back(name);
    }
    REQUIRE(names.size() == 3);
    for (const auto& name : names) REQUIRE(r.add(std::make_unique<NamedTool>(name)));
    for (const auto& name : names) {
        REQUIRE(r.find(name) != nullptr);
        CHECK(r.find(name)->name() == name);
    }
    CHECK(r.max_probe_length() == 3);
    CHECK(!r.add(std::make_unique<NamedTool>(names[1])));
    CHECK(r.size() == 3);
}

TEST(reg_doubles_before_load_exceeds_one) {
    ToolRegistry r;
    const std::size_t initial = r.bucket_count();
    for (std::size_t i = 0; i < initial; ++i) {
        REQUIRE(r.add(std::make_unique<NamedTool>("n" + std::to_string(i))));
        CHECK(r.load_factor() <= 1.0);
    }
    CHECK(r.bucket_count() == initial);
    CHECK(r.load_factor() == 1.0);
    Tool* original = r.find("n0");
    CHECK(!r.add(std::make_unique<NamedTool>("n0")));
    CHECK(r.bucket_count() == initial);
    CHECK(r.size() == initial);
    REQUIRE(r.add(std::make_unique<NamedTool>("one_more")));
    CHECK(r.bucket_count() == initial * 2);
    CHECK(r.size() == initial + 1);
    CHECK(r.find("n0") == original);
}

TEST(reg_destroys_owned_tools_once) {
    int destroyed = 0;
    int duplicate_destroyed = 0;
    {
        ToolRegistry r;
        for (int i = 0; i < 300; ++i) {
            REQUIRE(r.add(std::make_unique<CountedTool>("owned_" + std::to_string(i), destroyed)));
        }
        Tool* original = r.find("owned_0");
        CHECK(destroyed == 0);
        CHECK(!r.add(std::make_unique<CountedTool>("owned_0", duplicate_destroyed)));
        CHECK(duplicate_destroyed == 1);
        CHECK(destroyed == 0);
        CHECK(r.find("owned_0") == original);
        CHECK(r.size() == 300);
    }
    CHECK(destroyed == 300);
    CHECK(duplicate_destroyed == 1);
}

TEST(reg_10000_tools_and_alternative_hash) {
    ToolRegistry r;
    std::vector<Tool*> pointers;
    std::set<std::size_t> hashes;
    for (int i = 0; i < 10000; ++i) {
        const std::string name = "tool_" + std::to_string(i);
        auto tool = std::make_unique<NamedTool>(name);
        pointers.push_back(tool.get());
        REQUIRE(r.add(std::move(tool)));
        CHECK(r.size() == static_cast<std::size_t>(i + 1));
        CHECK(r.load_factor() <= 1.0);
        hashes.insert(ToolRegistry::hash(name));
    }
    for (int i = 0; i < 10000; ++i) {
        CHECK(r.find("tool_" + std::to_string(i)) == pointers[i]);
    }
    REQUIRE(r.find("tool_0") != nullptr);
    CHECK(r.find("tool_0")->name() == "tool_0");
    CHECK(r.find("tool_10000") == nullptr);
    CHECK(r.max_probe_length() <= 64);
    CHECK(hashes.size() >= 9990);
    CHECK(r.load_factor() == static_cast<double>(r.size()) / r.bucket_count());

    std::vector<int> weak_counts(r.bucket_count(), 0);
    std::vector<int> real_counts(r.bucket_count(), 0);
    for (int i = 0; i < 10000; ++i) {
        const std::string name = "tool_" + std::to_string(i);
        const std::size_t weak = static_cast<unsigned char>(name[0]) + name.size();
        ++weak_counts[weak % r.bucket_count()];
        ++real_counts[ToolRegistry::hash(name) % r.bucket_count()];
    }
    int weak_longest = 0;
    int real_longest = 0;
    for (std::size_t i = 0; i < r.bucket_count(); ++i) {
        if (weak_counts[i] > weak_longest) weak_longest = weak_counts[i];
        if (real_counts[i] > real_longest) real_longest = real_counts[i];
    }
    CHECK(r.max_probe_length() == static_cast<std::size_t>(real_longest));
    CHECK(weak_longest == 9000);
    std::printf("  10000 tools: buckets=%zu load=%.8f longest=%zu unique_hashes=%zu\n",
                r.bucket_count(), r.load_factor(), r.max_probe_length(), hashes.size());
    std::printf("  weak hash at %zu buckets: longest=%d\n", r.bucket_count(), weak_longest);
}

TEST(reg_uses_whole_key) {
    CHECK(ToolRegistry::hash("ab") != ToolRegistry::hash("ba"));
    CHECK(ToolRegistry::hash("tool_1234") != ToolRegistry::hash("tool_1243"));
    const std::string base = "every_character_matters";
    for (std::size_t i = 0; i < base.size(); ++i) {
        std::string changed = base;
        changed[i] = 'X';
        CHECK(ToolRegistry::hash(changed) != ToolRegistry::hash(base));
    }
    ToolRegistry r;
    const std::vector<std::string> names = {"", "prefix", "prefix_longer",
        std::string(512, 'x'), std::string("a") + '\0' + "b"};
    for (const auto& name : names) REQUIRE(r.add(std::make_unique<NamedTool>(name)));
    for (const auto& name : names) {
        REQUIRE(r.find(name) != nullptr);
        CHECK(r.find(name)->name() == name);
    }
    const std::string padded = "prefix trailing bytes";
    CHECK(r.find(std::string_view(padded.data(), 6)) == r.find("prefix"));
    CHECK(r.find("a") == nullptr);
}


TEST(ws_normalizes_relative_paths) {
    TempDir d;
    Workspace ws(d.path());
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"notes/todo.txt", "notes/todo.txt"},
        {"./notes//todo.txt", "notes/todo.txt"},
        {"a/../b", "b"},
        {"my..notes.txt", "my..notes.txt"},
        {"folder with spaces/a.txt", "folder with spaces/a.txt"},
    };
    for (const auto& item : cases) {
        fs::path out;
        std::string reason = "previous error";
        REQUIRE(ws.resolve(item.first, &out, &reason));
        CHECK(out == ws.root() / item.second);
        CHECK(reason.empty());
    }
}

TEST(ws_dot_and_cancelled_directory_are_root) {
    TempDir d;
    Workspace ws(d.path());
    for (const char* text : {".", "a/..", "./", "a/b/../.."}) {
        fs::path out;
        std::string reason;
        REQUIRE(ws.resolve(text, &out, &reason));
        CHECK(out == ws.root());
    }
}

TEST(ws_accepts_missing_paths_without_creating_them) {
    TempDir d;
    // Even the workspace itself need not exist for lexical resolution.
    Workspace ws(d.path() / "not_created");
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("new/dir/file.txt", &out, &reason));
    CHECK(out == ws.root() / "new/dir/file.txt");
    CHECK(!fs::exists(out));
    CHECK(!fs::exists(ws.root()));
}

TEST(ws_rejects_empty_path) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out = "unchanged";
    std::string reason;
    CHECK(!ws.resolve("", &out, &reason));
    CHECK(reason == "empty path");
    CHECK(out == "unchanged");
}

TEST(ws_rejects_embedded_nul) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out = "unchanged";
    std::string reason;
    const std::string path = std::string("notes") + '\0' + "/x";
    CHECK(!ws.resolve(path, &out, &reason));
    CHECK(!reason.empty());
    CHECK(out == "unchanged");
}

TEST(ws_rejects_absolute_paths) {
    TempDir d;
    Workspace ws(d.path());
    for (const auto& text : std::vector<std::string>{"/etc/passwd", "/", (ws.root() / "inside.txt").string()}) {
        fs::path out;
        std::string reason;
        CHECK(!ws.resolve(text, &out, &reason));
        CHECK(!reason.empty());
    }
}

TEST(ws_rejects_dotdot_and_reentry) {
    TempDir d;
    Workspace ws(d.path() / "ws");
    for (const char* text : {"..", "../etc/passwd", "a/../../b", "notes/../../ws/notes/todo.txt"}) {
        fs::path out = "unchanged";
        std::string reason;
        CHECK(!ws.resolve(text, &out, &reason));
        CHECK(!reason.empty());
        CHECK(out == "unchanged");
    }
}

TEST(ws_file_tools_reject_escapes) {
    TempDir d;
    ToolContext ctx(Workspace(d.path() / "ws"));
    fs::create_directory(ctx.workspace.root());
    const fs::path outside = d.path() / "outside.txt";
    write_text(outside, "keep this");
    ReadFileTool reader;
    WriteFileTool writer;
    ListDirTool listing;
    JsonValue args = args1("path", JsonValue::string("../outside.txt"));
    CHECK(!reader.invoke(args, ctx).ok);
    args.set("content", JsonValue::string("changed"));
    CHECK(!writer.invoke(args, ctx).ok);
    CHECK(read_text(outside) == "keep this");
    CHECK(!listing.invoke(args1("path", JsonValue::string("..")), ctx).ok);
}

TEST(ws_symlinks_are_checked_after_resolution) {
    TempDir d;
    ToolContext ctx(Workspace(d.path() / "ws"));
    fs::create_directory(ctx.workspace.root());
    write_text(d.path() / "outside.txt", "outside");
    fs::create_symlink(d.path() / "outside.txt", ctx.workspace.root() / "link");
    fs::create_symlink(d.path() / "missing.txt", ctx.workspace.root() / "dangling");
    fs::path out;
    std::string reason;
    REQUIRE(ctx.workspace.resolve("link", &out, &reason));
    CHECK(out == ctx.workspace.root() / "link");
    CHECK(!ctx.workspace.check_symlinks(out, &reason));
    CHECK(!reason.empty());
    ReadFileTool reader;
    CHECK(!reader.invoke(args1("path", JsonValue::string("link")), ctx).ok);
    WriteFileTool writer;
    JsonValue args = args1("path", JsonValue::string("dangling"));
    args.set("content", JsonValue::string("must not be written"));
    CHECK(!writer.invoke(args, ctx).ok);
    CHECK(!fs::exists(d.path() / "missing.txt"));
}


TEST(calc_left_to_right) {
    double value = 0;
    std::string error;
    REQUIRE(eval("10 - 4 - 3", &value, &error));
    CHECK(value == 3);
    REQUIRE(eval("100 / 10 / 5", &value, &error));
    CHECK(value == 2);
    REQUIRE(eval("12 / 3 * 2", &value, &error));
    CHECK(value == 8);
}

TEST(calc_parentheses) {
    double value = 0;
    std::string error;
    REQUIRE(eval("(2 + 3) * 4", &value, &error));
    CHECK(value == 20);
    REQUIRE(eval("2 * (3 + 4) - 5", &value, &error));
    CHECK(value == 9);
    REQUIRE(eval("((8 - 2) / (1 + 2)) * 5", &value, &error));
    CHECK(value == 10);
}

TEST(calc_negative_numbers) {
    double value = 0;
    std::string error;
    REQUIRE(eval("-3 * -3", &value, &error));
    CHECK(value == 9);
    REQUIRE(eval("2 * -3 + (-1)", &value, &error));
    CHECK(value == -7);
    REQUIRE(eval("5 - -2", &value, &error));
    CHECK(value == 7);
}

TEST(calc_decimals_and_division) {
    double value = 0;
    std::string error;
    REQUIRE(eval("7 / 2", &value, &error));
    CHECK(value == 3.5);
    REQUIRE(eval("1.25 * 4 + 0.5", &value, &error));
    CHECK(value == 5.5);
    REQUIRE(eval(" ( 9.5 - 2.5 ) / 2\t", &value, &error));
    CHECK(value == 3.5);
}

TEST(calc_zero_in_subexpression) {
    double value = 123;
    std::string error;
    for (const char* expression : {"1 / (2 - 2)", "7 / -0", "0 / 0", "2 * (3 / (1 - 1))"}) {
        error.clear();
        CHECK(!eval(expression, &value, &error));
        CHECK(error == "division by zero");
        CHECK(value == 123);
    }
}

TEST(calc_twenty_thousand_terms) {
    std::string expression = "1";
    for (int i = 1; i < 20000; ++i) expression += "+1";
    double value = 0;
    std::string error;
    REQUIRE(eval(expression, &value, &error));
    CHECK(value == 20000);
}

TEST(calc_five_thousand_parentheses) {
    const std::string expression = std::string(5000, '(') + "2+3" + std::string(5000, ')');
    double value = 0;
    std::string error;
    REQUIRE(eval(expression, &value, &error));
    CHECK(value == 5);
}

TEST(calc_bad_tokens_do_not_underflow) {
    using K = CalcToken::Kind;
    const std::vector<std::vector<CalcToken>> cases = {
        {},
        {{K::Plus, 0}},
        {{K::RParen, 0}},
        {{K::Number, 1}, {K::Plus, 0}},
        {{K::LParen, 0}, {K::Number, 1}},
        {{K::Number, 1}, {K::RParen, 0}},
        {{K::LParen, 0}, {K::RParen, 0}},
        {{K::Number, 1}, {K::Number, 2}},
        {{K::Number, 1}, {K::LParen, 0}, {K::Number, 2}, {K::RParen, 0}},
        {{K::Number, 1}, {K::Plus, 0}, {K::Times, 0}, {K::Number, 2}},
    };
    for (const auto& tokens : cases) {
        double value = 123;
        std::string error;
        CHECK(!calc_evaluate(tokens, &value, &error));
        CHECK(!error.empty());
        CHECK(value == 123);
    }
}

TEST(calc_single_value_and_recovery) {
    double value = 0;
    std::string error;
    CHECK(!eval("1/0", &value, &error));
    REQUIRE(eval("-42.5", &value, &error));
    CHECK(value == -42.5);
    CHECK(error.empty());
    REQUIRE(eval("0", &value, &error));
    CHECK(value == 0);
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
