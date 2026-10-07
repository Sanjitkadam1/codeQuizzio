#include "ProgressStore.h"

#include <fstream>
#include <sstream>
#include <system_error>

namespace cq {

namespace fs = std::filesystem;

namespace {

fs::path withSuffix(const fs::path& file, const char* suffix) {
    fs::path p = file;
    p += suffix;
    return p;
}

std::string readFile(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + file.string());
    std::ostringstream buf;
    buf << in.rdbuf();
    if (in.bad()) throw std::runtime_error("error reading " + file.string());
    return buf.str();
}

bool pathExists(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec);
}

}  // namespace

LoadResult loadProgress(const fs::path& file) {
    const fs::path backup = withSuffix(file, ".bak");
    LoadResult result;

    if (!pathExists(file) && !pathExists(backup)) return result;  // first run

    std::string problem;
    if (pathExists(file)) {
        try {
            result.progress = Progress::fromJson(readFile(file));
            result.source = LoadResult::Source::Primary;
            return result;
        } catch (const std::exception& e) {
            problem = file.filename().string() + " is unusable (" + e.what() + ")";
            // Keep the bad file for inspection instead of silently overwriting it.
            std::error_code ec;
            fs::rename(file, withSuffix(file, ".corrupt"), ec);
        }
    } else {
        problem = file.filename().string() + " is missing";
    }

    if (pathExists(backup)) {
        try {
            result.progress = Progress::fromJson(readFile(backup));
            result.source = LoadResult::Source::Backup;
            result.warning = problem + "; recovered from the previous save";
            return result;
        } catch (const std::exception& e) {
            problem += "; the backup is also unusable (" + std::string(e.what()) + ")";
        }
    }

    result.progress = Progress{};
    result.source = LoadResult::Source::Fresh;
    result.warning = problem + "; starting with empty progress";
    return result;
}

void saveProgress(const fs::path& file, const Progress& progress) {
    std::error_code ec;
    if (file.has_parent_path()) {
        fs::create_directories(file.parent_path(), ec);
        if (ec) throw std::runtime_error("cannot create " + file.parent_path().string() + ": " + ec.message());
    }

    const fs::path tmp = withSuffix(file, ".tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("cannot write " + tmp.string());
        out << progress.toJson();
        out.flush();
        if (!out) {
            out.close();
            fs::remove(tmp, ec);
            throw std::runtime_error("failed while writing " + tmp.string());
        }
    }

    // The current save becomes the backup only once the new data is safely on disk.
    if (pathExists(file)) {
        fs::copy_file(file, withSuffix(file, ".bak"), fs::copy_options::overwrite_existing, ec);
        if (ec) {
            fs::remove(tmp, ec);
            throw std::runtime_error("cannot update backup: " + ec.message());
        }
    }

    fs::rename(tmp, file, ec);  // replaces the destination atomically on Windows and POSIX
    if (ec) {
        std::error_code ignored;
        fs::remove(tmp, ignored);
        throw std::runtime_error("cannot replace " + file.string() + ": " + ec.message());
    }
}

}  // namespace cq
