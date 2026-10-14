// SPDX-License-Identifier: GPL-3.0-or-later
// The game files folder: keys/, firmware/ and roms/ in the folder chosen in Settings > Game
// files (/data/prosperoeden by default).
// Resolved once per process; a new choice applies when ProsperoEden is reopened. Where the rest
// lives: storage_paths.h; what is saved: settings_store.h.
// Keys, firmware and saves can be chosen apart from it, and more game folders added
// (Settings > Game files); each extra folder holds roms/, updates/ and mods/.
// other game folders are looked at in order (their keys/ and firmware/, or the folder itself).
#pragma once
#include <algorithm>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "game_place.h"
#include "mods.h"
#include "settings_store.h"
#include "storage_paths.h"

namespace Eden {
// The saved folder, else /data/prosperoeden, except that an install from before the setting
// keeps the app folder's assets/ while /data/prosperoeden has no keys (main.cpp moves those
// files to /data/prosperoeden when it can). Without filesystem access nothing can be read.
inline std::string ResolveAssetsDir() {
    const std::string legacy = AppFile("assets");
    if (!FilesystemAccess()) return kDefaultAssetsDir;
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

// ---- keys, firmware and saves, apart from the game files folder ----
// Ignored without filesystem access. Unchosen keys and firmware follow the game files folder,
// then the other game folders.
inline bool HasKeys(const std::string& directory) { return FileExists(directory + "/prod.keys"); }
inline bool HasFirmware(const std::string& directory) {
    for (const auto& entry : Mods::ListFolder(directory))
        if (Mods::Lower(entry.path().extension().string()) == ".nca") return true;
    return false;
}
inline std::vector<std::string> LibraryDirs();
// The first other game folder that has it: its keys/ or firmware/, or the folder itself.
template <class Has>
inline std::string FindInGameFolders(std::string_view child, Has has) {
    for (const std::string& directory : LibraryDirs()) {
        const std::string inside = directory + "/" + std::string(child);
        if (has(inside)) return inside;
        if (has(directory)) return directory;
    }
    return {};
}
inline std::string ResolveStoragePath(StoragePath which, std::string_view child) {
    if (FilesystemAccess()) {
        if (std::string saved = LoadSavedStoragePath(which); !saved.empty()) return saved;
        const std::string own = AssetsPath(child);
        if (which == StoragePath::keys && !HasKeys(own))
            if (std::string other = FindInGameFolders(child, HasKeys); !other.empty()) return other;
        if (which == StoragePath::firmware && !HasFirmware(own))
            if (std::string other = FindInGameFolders(child, HasFirmware); !other.empty()) return other;
    }
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
// Where saves are imported from (one folder per title ID) and exported to.
inline const std::string& SaveImportPath() {
    static const std::string directory = ResolveStoragePath(StoragePath::save_import, "save-import");
    return directory;
}
inline const std::string& SaveExportPath() {
    static const std::string directory = ResolveStoragePath(StoragePath::save_export, "save-export");
    return directory;
}

// ---- game folders: the game files folder, then the ones added in Settings ----
// A game with the same file name in two of them is listed once, from the first.
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
// The mods/ of every game folder, the game files folder's first.
inline std::vector<std::string> ModsDirs() {
    std::vector<std::string> result;
    for (const std::string& directory : LibraryDirs()) result.push_back(directory + "/mods");
    return result;
}
// Where a game's mods from several folders are linked together (mods.h).
inline std::string ModsStagingDir() { return UserDir() + "/mods-merged"; }
// The folder Eden reads this game's mods from; empty when there are none.
inline std::string ModsLoadDir(uint64_t title_id, bool* linked_all = nullptr) {
    return Mods::LoadRootFor(ModsDirs(), ModsStagingDir(), title_id, linked_all);
}
// The game's mods, from every game folder.
inline std::vector<Mods::Mod> ModsOf(uint64_t title_id) { return Mods::ListIn(ModsDirs(), title_id); }
// The path of a game file in the first game folder that has it.
inline std::string FindRom(std::string_view filename) {
    if (!ValidRomFilename(filename)) return {};
    for (const std::string& directory : RomDirs()) {
        const std::string path = directory + "/" + std::string(filename);
        if (FileExists(path)) return path;
    }
    return {};
}
} // namespace Eden
