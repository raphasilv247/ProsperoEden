// SPDX-License-Identifier: GPL-3.0-or-later
// Where a game folder is, in a word: the Library shows it after the file format ("NSP | USB").
//   /mnt/usb0, /mnt/usb1 ...   USB   (USB0, USB1 ... where two have to be told apart)
//   /mnt/ext0, /ext1 ...       EXT   (EXT0, EXT1 ...)
//   /data and the rest of the console's own storage   SSD
//   any other mount under /mnt   its own name, capitals
// MountRoot is the folder that holds the drive ("/mnt/usb0", "/data"): all the player is told of
// a game's place when one game is found in two of them. Never a full path.
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

// "/mnt/usb0/games" -> "/mnt/usb0"; "/ext0/games" -> "/ext0"; "/data/prosperoeden" -> "/data".
inline std::string MountRoot(std::string_view path) {
    const auto parts = place_detail::Parts(path);
    if (parts.empty()) return "/";
    if (parts[0] == "mnt" && parts.size() > 1) return "/mnt/" + parts[1];
    return "/" + parts[0];
}

// numbered: "USB0" instead of "USB", for telling two drives of one kind apart.
inline std::string PlaceLabel(std::string_view path, bool numbered = false) {
    const std::string name = place_detail::DriveName(place_detail::Parts(path));
    if (name.empty()) return "SSD";
    const std::string lower = place_detail::Lower(name);
    for (const char* kind : {"usb", "ext"}) {
        const std::string prefix = kind;
        if (lower.rfind(prefix, 0) == 0)
            return place_detail::Upper(prefix) + (numbered ? name.substr(prefix.size()) : std::string{});
    }
    std::string label = place_detail::Upper(name);
    if (label.size() > 8) label.resize(8);
    return label;
}

} // namespace Eden
