#pragma once

#include <filesystem>
#include <string>

#include "Progress.h"

namespace cq {

// Crash-safe progress storage. For a file `progress.json` it maintains:
//   progress.json          the current save
//   progress.json.bak      the previous good save (state before the last write)
//   progress.json.tmp      scratch file used while writing
//   progress.json.corrupt  a save that failed to parse, kept for inspection

struct LoadResult {
    enum class Source {
        Fresh,    // nothing usable on disk; started with empty progress
        Primary,  // loaded progress.json
        Backup,   // progress.json was unusable; recovered from progress.json.bak
    };
    Progress progress;
    Source source = Source::Fresh;
    std::string warning;  // empty when everything was fine
};

// Never throws for bad or missing data: falls back to the backup, then to empty
// progress, and describes what happened in `warning`.
LoadResult loadProgress(const std::filesystem::path& file);

// Writes to a temp file, keeps the previous save as .bak, then renames the temp
// file into place, so a crash mid-save can never leave a half-written
// progress.json. (Surviving sudden power loss would additionally need an fsync,
// which the standard library does not offer.) Throws std::runtime_error on I/O
// failure; the existing save is left untouched in that case.
void saveProgress(const std::filesystem::path& file, const Progress& progress);

}  // namespace cq
