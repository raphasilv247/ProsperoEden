// SPDX-License-Identifier: GPL-3.0-or-later
// The game files folder: keys/, firmware/ and roms/ in the folder chosen in Settings > Game
// files (/data/prosperoeden by default), or the app folder's assets/ without filesystem access.
// Resolved once per process; a new choice applies when ProsperoEden is reopened. Where the rest
// lives: storage_paths.h; what is saved: settings_store.h.
//
// Two things can be chosen apart from it (Settings > Game files):
//  - keys, firmware, save-import and save-export, each on its own (the Advanced rows). Until
//    one is chosen it is the game files folder's keys/, firmware/, save-import/ or save-export/.
//    Resolved once per process, like the game files folder.
//  - more game folders, each with only roms/ and updates/. The game files folder is always the
//    first of them. They apply at once: the Library reads them again each time it opens.
#pragma once
#include <algorithm>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "settings_store.h"
#include "storage_paths.h"

namespace Eden {
// Without filesystem access only the app folder's assets/ is readable. With it: the saved
// folder, else /data/prosperoeden, except that an install from before the setting keeps the
// app folder's assets/ while /data/prosperoeden has no keys.
inline std::string ResolveAssetsDir() {
    const std::string legacy = AppFile("assets");
    if (!FilesystemAccess()) return legacy;
    if (std::string saved = LoadSavedAssetsDir(); !saved.empty()) return saved;
    const std::string keys = "/keys/prod.keys";
    if (!FileExists(kDefaultAssetsDir + keys) && FileExists(legacy + keys)) return legacy;
    return kDefaultAssetsDir;
}

// The game files folder in use by this process.
inline const std::string& AssetsDir() {
    static const std::string directory = ResolveAssetsDir();
    return directory;
}
inline std::string AssetsPath(std::string_view child) { return AssetsDir() + "/" + std::string(child); }

// ---- keys, firmware and saves, each apart from the game files folder when chosen ----
// Without filesystem access only the app folder's assets/ is readable, so a choice is ignored.
inline std::string ResolveStoragePath(StoragePath which, std::string_view child) {
    if (FilesystemAccess())
        if (std::string saved = LoadSavedStoragePath(which); !saved.empty()) return saved;
    return AssetsPath(child);
}
// The folder that holds prod.keys (and title.keys).
inline const std::string& KeysPath() {
    static const std::string directory = ResolveStoragePath(StoragePath::keys, "keys");
    return directory;
}
// The folder that holds the extracted firmware NCAs.
inline const std::string& FirmwarePath() {
    static const std::string directory = ResolveStoragePath(StoragePath::firmware, "firmware");
    return directory;
}
// Where saves to import are looked for (one folder per title ID), and where exports are written.
inline const std::string& SaveImportPath() {
    static const std::string directory = ResolveStoragePath(StoragePath::save_import, "save-import");
    return directory;
}
inline const std::string& SaveExportPath() {
    static const std::string directory = ResolveStoragePath(StoragePath::save_export, "save-export");
    return directory;
}

// ---- game folders: the game files folder, then the ones added in Settings ----
// Each holds roms/ and updates/ (the game files folder holds the rest too). A game of the same
// file name in two of them is listed once, from the first.
struct LibraryCache {
    std::mutex mutex;
    bool loaded = false;
    std::vector<std::string> directories;
};
inline LibraryCache& Library() {
    static LibraryCache cache;
    return cache;
}
inline std::vector<std::string> LibraryDirs() {
    LibraryCache& cache = Library();
    const std::lock_guard lock(cache.mutex);
    if (!cache.loaded) {
        cache.directories = {AssetsDir()};
        if (FilesystemAccess())
            for (std::string& directory : LoadLibraryDirs())
                if (std::find(cache.directories.begin(), cache.directories.end(), directory) == cache.directories.end())
                    cache.directories.push_back(std::move(directory));
        cache.loaded = true;
    }
    return cache.directories;
}
// The saved list changed: read it again at the next look.
inline void ReloadLibraryDirs() {
    LibraryCache& cache = Library();
    const std::lock_guard lock(cache.mutex);
    cache.loaded = false;
}
inline std::vector<std::string> RomDirs() {
    std::vector<std::string> result;
    for (const std::string& directory : LibraryDirs()) result.push_back(directory + "/roms");
    return result;
}
inline std::vector<std::string> UpdateDirs() {
    std::vector<std::string> result;
    for (const std::string& directory : LibraryDirs()) result.push_back(directory + "/updates");
    return result;
}
// The full path of a game file in the first game folder that has it; empty when none does.
inline std::string FindRom(std::string_view filename) {
    if (!ValidRomFilename(filename)) return {};
    for (const std::string& directory : RomDirs()) {
        const std::string path = directory + "/" + std::string(filename);
        if (FileExists(path)) return path;
    }
    return {};
}
} // namespace Eden
