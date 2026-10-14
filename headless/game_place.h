// SPDX-License-Identifier: GPL-3.0-or-later
// Short label for where a game folder is, shown after the file format ("NSP | EXT0"):
//   /mnt/usb0 -> USB0, /mnt/ext1 -> EXT1, /data and other console storage -> SSD, other /mnt/x -> X
#pragma once
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace Eden {

namespace place_detail {
inline std::vector<std::string> Parts(std::string_view path) {
    std::vector<std::string> parts;
    std::string current;
    for (const char c : path) {
        if (c == '/') {
            if (!current.empty()) parts.push_back(std::move(current));
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) parts.push_back(std::move(current));
    return parts;
}
inline std::string Lower(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
inline std::string Upper(std::string text) {
    for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return text;
}
// The name of the drive's folder ("usb0", "ext1"), empty for the console's own storage.
inline std::string DriveName(const std::vector<std::string>& parts) {
    if (parts.empty()) return {};
    if (parts[0] == "mnt") return parts.size() > 1 ? parts[1] : std::string{};
    const std::string first = Lower(parts[0]);
    if (first.rfind("usb", 0) == 0 || first.rfind("ext", 0) == 0) return parts[0];
    return {};
}
} // namespace place_detail

// "/ext0/games/roms" -> "/ext0/games": the game files folder of a roms/ folder.
inline std::string GameFolderOf(std::string_view roms) {
    const std::string_view suffix = "/roms";
    if (roms.size() >= suffix.size() && roms.substr(roms.size() - suffix.size()) == suffix)
        roms.remove_suffix(suffix.size());
    return std::string{roms};
}

// The drive's number follows the kind: "USB0", "USB1", "EXT0", "EXT1" (for telling two drives of one kind apart).
inline std::string PlaceLabel(std::string_view path) {
    const std::string name = place_detail::DriveName(place_detail::Parts(path));
    if (name.empty()) return "SSD";
    const std::string lower = place_detail::Lower(name);
    for (const char* kind : {"usb", "ext"}) {
        const std::string prefix = kind;
        if (lower.rfind(prefix, 0) == 0)
            return place_detail::Upper(prefix) + name.substr(prefix.size());
    }
    std::string label = place_detail::Upper(name);
    if (label.size() > 8) label.resize(8);
    return label;
}

} // namespace Eden
