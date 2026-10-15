#include "tools/workspace.h"

namespace fs = std::filesystem;

static bool reject_path(std::string* reason, const char* message) {
    if (reason != nullptr) *reason = message;
    return false;
}

bool Workspace::resolve(std::string_view user_path, fs::path* out,
                        std::string* reason) const {
    try {
        if (user_path.empty()) return reject_path(reason, "empty path");
        if (user_path.find('\0') != std::string_view::npos) {
            return reject_path(reason, "path contains a NUL byte");
        }
        fs::path p{std::string(user_path)};
        if (p.is_absolute() || p.has_root_path()) {
            return reject_path(reason, "path must be relative to the workspace");
        }

        const fs::path normal = p.lexically_normal();
        if (normal.begin() != normal.end() && *normal.begin() == "..") {
            return reject_path(reason, "path escapes the workspace");
        }
        if (out == nullptr) return reject_path(reason, "missing output path");
        if (normal == ".") *out = root();
        else *out = root() / normal;
        if (reason != nullptr) reason->clear();
        return true;
    } catch (...) {
        // Path construction can fail. Keep failures at this boundary as data.
        // Even reporting an error can fail if memory is exhausted.
        try { reject_path(reason, "invalid path"); } catch (...) {}
        return false;
    }
}
