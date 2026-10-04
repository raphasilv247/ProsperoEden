// ProsperoEden - Launcher: the Game files folder browser, Language and About.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pe/ui/launcher.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace pe::ui
{

using audio::Cue;

namespace
{

constexpr Rect kListPanel{108.0f, 188.0f, 820.0f, 720.0f};
constexpr Rect kDetailPanel{980.0f, 188.0f, 820.0f, 720.0f};
constexpr Rect kWindow{138.0f, 292.0f, 760.0f, 518.0f};
constexpr float kRowHeight = 78.0f;

std::string join_path(const std::string &directory, const std::string &name)
{
    return directory == "/" ? "/" + name : directory + "/" + name;
}

std::string parent_path(const std::string &directory)
{
    const std::size_t slash = directory.find_last_of('/');
    return slash == 0 || slash == std::string::npos ? "/" : directory.substr(0, slash);
}

std::string short_path(const std::string &path, std::size_t limit)
{
    return path.size() <= limit ? path : "..." + path.substr(path.size() - (limit - 3));
}

// "3 games": the count goes where the text says {0}; one and many are separate texts, so a
// language can word each as it needs.
std::string counted(int count, const char *one, const char *many)
{
    return fill(count == 1 ? one : many, {std::to_string(count)});
}

// A folder mark from two rounded shapes; `up` adds the arrow of the parent folder.
void draw_folder(Canvas &c, float x, float cy, Color ink, bool up)
{
    c.list.rounded_rect({x, cy - 13.0f, 18.0f, 8.0f}, 3.0f, ink);
    c.list.rounded_rect({x, cy - 9.0f, 40.0f, 24.0f}, 4.0f, ink);
    if (up)
    {
        const Color dark = Color::rgb(0x12201a);
        c.list.line(x + 20.0f, cy + 9.0f, x + 20.0f, cy - 3.0f, 2.4f, dark);
        c.list.line(x + 14.0f, cy + 2.0f, x + 20.0f, cy - 4.0f, 2.4f, dark);
        c.list.line(x + 26.0f, cy + 2.0f, x + 20.0f, cy - 4.0f, 2.4f, dark);
    }
}

} // namespace

// ---------------------------------------------------------------- game files

StorageRole Launcher::storage_role(Pick pick)
{
    switch (pick)
    {
    case Pick::keys:
        return StorageRole::keys;
    case Pick::firmware:
        return StorageRole::firmware;
    case Pick::save_import:
        return StorageRole::save_import;
    default:
        return StorageRole::save_export;
    }
}

Launcher::Pick Launcher::storage_pick(StorageRole role)
{
    switch (role)
    {
    case StorageRole::keys:
        return Pick::keys;
    case StorageRole::firmware:
        return Pick::firmware;
    case StorageRole::save_import:
        return Pick::save_import;
    default:
        return Pick::save_export;
    }
}

void Launcher::enter_files(Pick pick, int index)
{
    browse_pick_ = pick;
    browse_index_ = index;
    // Start where the player's folder is (or will be after reopening), else the nearest folder
    // that can be opened. A new game folder starts at the drives' mount points.
    std::string start;
    switch (pick)
    {
    case Pick::main:
        start = services_.saved_files_folder();
        if (start.empty())
            start = services_.files_folder();
        break;
    case Pick::add_folder:
        start = "/mnt";
        break;
    case Pick::change_folder:
        if (index >= 0 && index < static_cast<int>(library_folders_.size()))
            start = library_folders_[static_cast<std::size_t>(index)];
        break;
    default:
        start = services_.saved_storage_folder(storage_role(pick));
        if (start.empty())
            start = services_.storage_folder(storage_role(pick));
        break;
    }
    files_.visible = 6;
    files_.pitch = 88.0f;
    while (!browse_to(start) && start != "/")
        start = parent_path(start);
    message_.clear();
}

bool Launcher::browse_to(const std::string &directory)
{
    std::vector<std::string> folders;
    if (!services_.folders(directory, &folders))
        return false;
    browse_dir_ = directory;
    browse_entries_.clear();
    if (directory != "/")
        browse_entries_.push_back("..");
    browse_entries_.insert(browse_entries_.end(), folders.begin(), folders.end());
    files_.reset(static_cast<int>(browse_entries_.size()), 0);
    // The folder shown is what TRIANGLE saves: show what it holds.
    folder_info_ = services_.folder_info(directory);
    return true;
}

void Launcher::press_files(Key key)
{
    const int count = static_cast<int>(browse_entries_.size());
    switch (key)
    {
    case Key::circle:
        leave_browser(browse_from_);
        return;
    case Key::up:
    case Key::down:
        if (files_.move(key == Key::down ? 1 : -1))
            cue(Cue::focus);
        return;
    case Key::l1:
    case Key::r1:
        if (files_.page(key == Key::r1 ? 1 : -1))
            cue(Cue::page);
        return;
    case Key::cross:
    {
        if (count == 0)
            return;
        const std::string entry = browse_entries_[static_cast<std::size_t>(files_.selected)];
        const std::string from = browse_dir_;
        const bool up = entry == "..";
        if (!browse_to(up ? parent_path(browse_dir_) : join_path(browse_dir_, entry)))
        {
            say(tr("This folder cannot be opened."), true);
            cue(Cue::error);
            return;
        }
        message_.clear();
        // Going up keeps the folder just left in view.
        if (up)
        {
            const std::string left = from.substr(from.find_last_of('/') + 1);
            for (int i = 0; i < static_cast<int>(browse_entries_.size()); ++i)
                if (browse_entries_[static_cast<std::size_t>(i)] == left)
                {
                    files_.reset(files_.count, i);
                    break;
                }
        }
        cue(Cue::select);
        return;
    }
    case Key::triangle:
        use_browsed_folder();
        return;
    case Key::square:
        if (browse_pick_ == Pick::main)
        {
            const std::string fallback = services_.default_files_folder();
            const bool saved = services_.set_files_folder(fallback);
            (void)browse_to(fallback);
            say(saved ? tr("Default folder saved. Reopen ProsperoEden to use it.") :
                        tr("Could not save the folder. Please try again."),
                !saved);
            cue(saved ? Cue::saved : Cue::error);
        }
        else if (browse_pick_ != Pick::add_folder && browse_pick_ != Pick::change_folder)
            reset_browsed_storage();
        return;
    default:
        return;
    }
}

void Launcher::leave_browser(int select)
{
    open(Screen::locations, false);
    enter_locations(select);
}

void Launcher::use_browsed_folder()
{
    const auto fail = [this](const char *text)
    {
        say(text, true);
        cue(Cue::error);
    };
    switch (browse_pick_)
    {
    case Pick::main:
    {
        // The game files folder: it stays on screen, as it always has.
        const bool saved = services_.set_files_folder(browse_dir_);
        say(saved ? tr("Saved. Reopen ProsperoEden to use this folder.") :
                    tr("Could not save the folder. Please try again."),
            !saved);
        cue(saved ? Cue::saved : Cue::error);
        return;
    }
    case Pick::add_folder:
    case Pick::change_folder:
    {
        // Only roms/ is needed here (and updates/, which may come later): no keys, no firmware.
        if (folder_info_.games < 0)
            return fail(tr("This folder needs a roms folder."));
        if (browse_dir_ == services_.files_folder() || browse_dir_ == services_.saved_files_folder())
            return fail(tr("This is the game files folder already."));
        std::vector<std::string> folders = library_folders_;
        const bool change = browse_pick_ == Pick::change_folder && browse_index_ >= 0 &&
                            browse_index_ < static_cast<int>(folders.size());
        const auto same = std::find(folders.begin(), folders.end(), browse_dir_);
        if (same != folders.end() && !(change && same - folders.begin() == browse_index_))
            return fail(tr("This folder is in the list already."));
        if (change)
            folders[static_cast<std::size_t>(browse_index_)] = browse_dir_;
        else if (static_cast<int>(folders.size()) >= services_.library_folders_max())
            return fail(tr("The list is full. Remove a folder first."));
        else
            folders.push_back(browse_dir_);
        if (!services_.set_library_folders(folders))
            return fail(tr("Could not save the folder. Please try again."));
        const int row = static_cast<int>(std::find(folders.begin(), folders.end(), browse_dir_) - folders.begin());
        library_folders_changed();
        leave_browser(1 + row);
        say(tr("Saved. Its games are in the Library now."));
        cue(Cue::saved);
        return;
    }
    default:
    {
        // Keys and firmware must be in the folder itself, or the game would not start.
        if (browse_pick_ == Pick::keys && !folder_info_.own_keys)
            return fail(tr("prod.keys is not in this folder."));
        if (browse_pick_ == Pick::firmware && folder_info_.own_firmware <= 0)
            return fail(tr("This folder has no firmware NCA files."));
        if (!services_.set_storage_folder(storage_role(browse_pick_), browse_dir_))
            return fail(tr("Could not save the folder. Please try again."));
        leave_browser(browse_from_);
        say(tr("Saved. Reopen ProsperoEden to use this folder."));
        cue(Cue::saved);
        return;
    }
    }
}

void Launcher::reset_browsed_storage()
{
    if (!services_.set_storage_folder(storage_role(browse_pick_), std::string{}))
    {
        say(tr("Could not save the folder. Please try again."), true);
        cue(Cue::error);
        return;
    }
    leave_browser(browse_from_);
    say(tr("Back to the default folder. Reopen ProsperoEden to use it."));
    cue(Cue::saved);
}

void Launcher::draw_files(Canvas &c)
{
    gfx::DrawList &list = c.list;
    const int count = static_cast<int>(browse_entries_.size());
    switch (browse_pick_)
    {
    case Pick::add_folder:
        draw_frame(c, tr("Add game folder"), tr("Choose a folder with roms and updates inside"));
        break;
    case Pick::change_folder:
        draw_frame(c, tr("Change game folder"), tr("Choose a folder with roms and updates inside"));
        break;
    case Pick::keys:
        draw_frame(c, tr("Keys folder"), tr("Choose the folder that holds prod.keys"));
        break;
    case Pick::firmware:
        draw_frame(c, tr("Firmware folder"), tr("Choose the folder that holds the firmware NCA files"));
        break;
    case Pick::save_import:
        draw_frame(c, tr("Save import folder"), tr("Choose the folder with one folder per game ID"));
        break;
    case Pick::save_export:
        draw_frame(c, tr("Save export folder"), tr("Choose where exported saves are written"));
        break;
    default:
        draw_frame(c, tr("Game files"), tr("Choose the folder that holds your keys, firmware and games"));
        break;
    }

    // ---- folders ----
    glass(c, kListPanel, 26.0f, theme::kPanel.with_alpha(0.80f), theme::kPanelEdge.with_alpha(0.55f));
    text(c, tr("FOLDERS"), 138.0f, baseline(208.0f, 28.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::left, 3.0f);
    text(c, short_path(browse_dir_, 52), 138.0f, baseline(246.0f, 28.0f, theme::kSmall),
         theme::kSmall, theme::kBody);
    if (count == 0)
    {
        text(c, tr("No folders here."), kListPanel.x + kListPanel.w * 0.5f,
             baseline(488.0f, 38.0f, theme::kText24), theme::kText24, theme::kCopy, Align::center);
    }
    else
    {
        list.push_clip({kWindow.x - 24.0f, kWindow.y - 6.0f, kWindow.w + 48.0f, kWindow.h + 12.0f});
        const auto row_rect = [&](int row) -> Rect
        {
            return {kWindow.x, kWindow.y + static_cast<float>(row) * files_.pitch - files_.scroll(),
                    kWindow.w, kRowHeight};
        };
        for (int row = files_.first_row(); row <= files_.last_row(); ++row)
        {
            list.push_opacity(files_.row_alpha(row, kRowHeight));
            plate_rest(c, kListPlate, row_rect(row));
            list.pop_opacity();
        }
        plate_focus(c, kListPlate,
                    {kWindow.x, kWindow.y + files_.cursor() - files_.scroll(), kWindow.w, kRowHeight},
                    1.0f);
        for (int row = files_.first_row(); row <= files_.last_row(); ++row)
        {
            const std::string &entry = browse_entries_[static_cast<std::size_t>(row)];
            const bool up = entry == "..";
            const Rect r = row_rect(row);
            list.push_opacity(files_.row_alpha(row, kRowHeight));
            draw_folder(c, r.x + 24.0f, r.y + kRowHeight * 0.5f,
                        up ? theme::kLimePale : Color::rgb(0xc9d6bd), up);
            const float tag = text(c, up ? tr("UP") : tr("OPEN"), r.x + 730.0f,
                                   baseline(r.y, kRowHeight, theme::kSmall), theme::kSmall,
                                   theme::kMeta, Align::right);
            text_fit(c, up ? tr("Parent folder") : entry, r.x + 84.0f,
                     baseline(r.y, kRowHeight, theme::kText24), theme::kText24,
                     up ? theme::kLimePale : Color::rgb(0xf3f5e9), 730.0f - 84.0f - tag - 24.0f);
            list.pop_opacity();
        }
        list.pop_clip();
        scrollbar(c, files_, 910.0f, kWindow.y, kWindow.h);
    }
    text(c, list_position(count > 0 ? files_.selected + 1 : 0, count), 898.0f, baseline(866.0f, 28.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::right);

    // ---- what the folder shown holds ----
    glass(c, kDetailPanel, 26.0f, theme::kPanel.with_alpha(0.80f),
          theme::kPanelEdge.with_alpha(0.55f));
    text(c, tr("THIS FOLDER"), 1016.0f, baseline(210.0f, 28.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::left, 3.0f);
    text_block(c, browse_dir_, 1016.0f, baseline(250.0f, 42.0f, theme::kHeading), theme::kHeading,
               42.0f, theme::kTitle, 748.0f, 2);
    const float column = value_column(
        c, {tr("KEYS"), tr("FIRMWARE"), tr("GAMES"), tr("UPDATES, DLC"), tr("IN USE"), tr("ACCESS")}, 1016.0f,
        1216.0f, theme::kSmall, 2.0f);
    // A line whose value is in the warning colour carries the warning mark too.
    const auto line = [&](float top, const char *label, const std::string &value, Color color)
    {
        text(c, label, 1016.0f, baseline(top, 30.0f, theme::kSmall), theme::kSmall, theme::kLabel,
             Align::left, 2.0f);
        notice(c, value, column, baseline(top, 30.0f, theme::kSmall), theme::kSmall, color,
               1764.0f - column, color.r == theme::kWarning.r && color.g == theme::kWarning.g);
    };
    const auto state = [](bool ready) { return ready ? theme::kLimePale : theme::kWarning; };
    const std::string in_use_folder = services_.files_folder();
    const std::string saved = services_.saved_files_folder();
    const char *note = "";
    switch (browse_pick_)
    {
    case Pick::add_folder:
    case Pick::change_folder:
        // Only roms/ and updates/ are read from a game folder; updates/ may come later.
        line(378.0f, tr("GAMES"),
             folder_info_.games < 0 ? tr("No roms folder") :
                                      counted(folder_info_.games, tr("{0} game"), tr("{0} games")),
             state(folder_info_.games >= 0));
        line(434.0f, tr("UPDATES, DLC"),
             folder_info_.updates < 0 ? tr("No updates folder") :
                                        counted(folder_info_.updates, tr("{0} file"), tr("{0} files")),
             theme::kValue);
        note = tr("Only roms and updates are read from here. TRIANGLE adds the folder shown.");
        break;
    case Pick::keys:
        line(378.0f, tr("KEYS"), folder_info_.own_keys ? tr("prod.keys found") : tr("prod.keys missing"),
             state(folder_info_.own_keys));
        note = tr("The folder shown must hold prod.keys. TRIANGLE uses it; SQUARE goes back to the default.");
        break;
    case Pick::firmware:
        line(378.0f, tr("FIRMWARE"),
             folder_info_.own_firmware < 0 ? tr("This folder cannot be read") :
                 counted(folder_info_.own_firmware, tr("{0} NCA file"), tr("{0} NCA files")),
             state(folder_info_.own_firmware > 0));
        note = tr("The folder shown must hold the firmware NCA files. TRIANGLE uses it; SQUARE goes back to the default.");
        break;
    case Pick::save_import:
    case Pick::save_export:
    {
        const StorageRole role = storage_role(browse_pick_);
        const std::string current = services_.storage_folder(role);
        line(378.0f, tr("IN USE"), short_path(current, 40), theme::kValue);
        note = browse_pick_ == Pick::save_import ?
                   tr("Saves to import go here, one folder per game ID. TRIANGLE uses the folder shown; SQUARE goes back to the default.") :
                   tr("Exported saves are written here. TRIANGLE uses the folder shown; SQUARE goes back to the default.");
        break;
    }
    default:
        line(378.0f, tr("KEYS"), folder_info_.keys ? tr("prod.keys found") : tr("prod.keys missing"),
             state(folder_info_.keys));
        line(434.0f, tr("FIRMWARE"),
             folder_info_.firmware < 0 ? tr("No firmware folder") :
                                         counted(folder_info_.firmware, tr("{0} NCA file"), tr("{0} NCA files")),
             state(folder_info_.firmware > 0));
        line(490.0f, tr("GAMES"),
             folder_info_.games < 0 ? tr("No roms folder") : counted(folder_info_.games, tr("{0} game"), tr("{0} games")),
             state(folder_info_.games > 0));
        note = tr("Keep keys, firmware and roms folders together. TRIANGLE uses the folder shown.");
        break;
    }
    list.rounded_rect({1016.0f, 554.0f, 748.0f, 1.0f}, 0.0f, theme::kRule);
    if (browse_pick_ == Pick::main)
    {
        line(578.0f, tr("IN USE"),
             !saved.empty() && saved != in_use_folder ? fill(tr("Next launch: {0}"), {short_path(saved, 32)}) :
                                                        short_path(in_use_folder, 40),
             theme::kValue);
        const int access = services_.filesystem_access();
        line(634.0f, tr("ACCESS"),
             access == 0 ? tr("Full filesystem") :
                           fill(tr("Sandboxed (code {0}): app folder only"), {std::to_string(access)}),
             state(access == 0));
    }
    else
    {
        const int access = services_.filesystem_access();
        line(578.0f, tr("ACCESS"),
             access == 0 ? tr("Full filesystem") :
                           fill(tr("Sandboxed (code {0}): app folder only"), {std::to_string(access)}),
             state(access == 0));
    }
    list.rounded_rect({1016.0f, 698.0f, 748.0f, 1.0f}, 0.0f, theme::kRule);
    notice_block(c, message_.empty() ? std::string{note} : message_, 1016.0f,
                 baseline(722.0f, 36.0f, theme::kText24), theme::kText24, 36.0f,
                 message_.empty() ? theme::kCopy : message_warning_ ? theme::kWarning : theme::kLimePale, 748.0f, 3,
                 !message_.empty() && message_warning_);

    switch (browse_pick_)
    {
    case Pick::main:
    {
        static constexpr Hint kHints[] = {{Pad::cross, TR("Open")},
                                          {Pad::circle, TR("Back")},
                                          {Pad::triangle, TR("Use this folder")},
                                          {Pad::square, TR("Default folder")},
                                          {Pad::updown, TR("Browse")},
                                          {Pad::l1, TR("Page"), Pad::r1}};
        draw_footer(c, kHints, 6);
        break;
    }
    case Pick::add_folder:
    case Pick::change_folder:
    {
        static constexpr Hint kHints[] = {{Pad::cross, TR("Open")},
                                          {Pad::circle, TR("Back")},
                                          {Pad::triangle, TR("Use this folder")},
                                          {Pad::updown, TR("Browse")},
                                          {Pad::l1, TR("Page"), Pad::r1}};
        draw_footer(c, kHints, 5);
        break;
    }
    default:
    {
        static constexpr Hint kHints[] = {{Pad::cross, TR("Open")},
                                          {Pad::circle, TR("Back")},
                                          {Pad::triangle, TR("Use this folder")},
                                          {Pad::square, TR("Default folder")},
                                          {Pad::updown, TR("Browse")},
                                          {Pad::l1, TR("Page"), Pad::r1}};
        draw_footer(c, kHints, 6);
        break;
    }
    }
}

// ---------------------------------------------------------------- language

void Launcher::enter_language()
{
    language_.visible = 6;
    language_.pitch = 88.0f;
    language_.reset(static_cast<int>(services_.language_labels().size()), prefs_.language);
    message_.clear();
}

void Launcher::press_language(Key key)
{
    switch (key)
    {
    case Key::circle:
        open(Screen::settings, false);
        // The home screen names the language the last game will use, and so does the Library.
        read_home();
        name_home_games();
        finish_scan(false);
        start_scan();
        return;
    case Key::up:
    case Key::down:
        if (language_.move(key == Key::down ? 1 : -1))
        {
            message_.clear();
            cue(Cue::focus);
        }
        return;
    case Key::l1:
    case Key::r1:
        if (language_.page(key == Key::r1 ? 1 : -1))
        {
            message_.clear();
            cue(Cue::page);
        }
        return;
    case Key::cross:
    {
        const int before = prefs_.language;
        prefs_.language = language_.selected;
        const bool saved = services_.set_preferences(prefs_);
        if (!saved)
            prefs_.language = before;
        say(saved ? tr("Saved. Applies when a game starts.") : tr("Could not save. Please try again."),
            !saved);
        cue(saved ? Cue::saved : Cue::error);
        return;
    }
    default:
        return;
    }
}

void Launcher::draw_language(Canvas &c)
{
    gfx::DrawList &list = c.list;
    const auto &labels = services_.language_labels();
    const int count = static_cast<int>(labels.size());
    draw_frame(c, tr("Language"), tr("Choose the language games use"));

    glass(c, kListPanel, 26.0f, theme::kPanel.with_alpha(0.80f), theme::kPanelEdge.with_alpha(0.55f));
    text(c, tr("LANGUAGES"), 138.0f, baseline(208.0f, 28.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::left, 3.0f);
    list.push_clip({kWindow.x - 24.0f, kWindow.y - 6.0f, kWindow.w + 48.0f, kWindow.h + 12.0f});
    const auto row_rect = [&](int row) -> Rect
    {
        return {kWindow.x, kWindow.y + static_cast<float>(row) * language_.pitch - language_.scroll(),
                kWindow.w, kRowHeight};
    };
    for (int row = language_.first_row(); row <= language_.last_row(); ++row)
    {
        list.push_opacity(language_.row_alpha(row, kRowHeight));
        plate_rest(c, kListPlate, row_rect(row));
        list.pop_opacity();
    }
    plate_focus(c, kListPlate,
                {kWindow.x, kWindow.y + language_.cursor() - language_.scroll(), kWindow.w,
                 kRowHeight},
                1.0f);
    for (int row = language_.first_row(); row <= language_.last_row(); ++row)
    {
        const Rect r = row_rect(row);
        list.push_opacity(language_.row_alpha(row, kRowHeight));
        // The language in use carries a lime tag; its name has the room the tag leaves.
        const float width = text_width(c, tr("IN USE"), 18.0f, 2.0f) + 28.0f;
        text_shrink(c, labels[static_cast<std::size_t>(row)], r.x + 26.0f,
                    baseline(r.y, kRowHeight, theme::kText24), theme::kText24, Color::rgb(0xf3f5e9),
                    row == prefs_.language ? 734.0f - 26.0f - width - 24.0f : 708.0f);
        if (row == prefs_.language)
        {
            // The tag is as tall as its letters need.
            const float tall = std::max(30.0f, text_size(18.0f) + 10.0f);
            const Rect tag{r.x + 734.0f - width, r.y + (kRowHeight - tall) * 0.5f, width, tall};
            list.bordered_rect(tag, tall * 0.5f, theme::kLime.with_alpha(0.16f), 1.0f,
                               theme::kLime.with_alpha(0.55f));
            text(c, tr("IN USE"), tag.x + tag.w * 0.5f, baseline(tag.y, tall, 18.0f), 18.0f,
                 theme::kLimePale, Align::center, 2.0f);
        }
        list.pop_opacity();
    }
    list.pop_clip();
    scrollbar(c, language_, 910.0f, kWindow.y, kWindow.h);
    text(c, list_position(language_.selected + 1, count), 898.0f, baseline(866.0f, 28.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::right);

    glass(c, kDetailPanel, 26.0f, theme::kPanel.with_alpha(0.80f),
          theme::kPanelEdge.with_alpha(0.55f));
    text(c, tr("SELECTED"), 1016.0f, baseline(210.0f, 28.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::left, 3.0f);
    const auto label_at = [&](int index) -> std::string
    {
        return index >= 0 && index < count ? labels[static_cast<std::size_t>(index)] : std::string{};
    };
    text_shrink(c, label_at(language_.selected), 1016.0f, baseline(250.0f, 42.0f, theme::kHeading),
                theme::kHeading, theme::kTitle, 748.0f);
    const float column =
        value_column(c, {tr("REGION"), tr("IN USE")}, 1016.0f, 1216.0f, theme::kSmall, 2.0f);
    const auto line = [&](float top, const char *label, const std::string &value, Color color)
    {
        text(c, label, 1016.0f, baseline(top, 30.0f, theme::kSmall), theme::kSmall, theme::kLabel,
             Align::left, 2.0f);
        text_shrink(c, value, column, baseline(top, 30.0f, theme::kSmall), theme::kSmall, color,
                    1764.0f - column);
    };
    line(378.0f, tr("REGION"), services_.language_region(language_.selected), theme::kLimePale);
    line(434.0f, tr("IN USE"), label_at(prefs_.language), theme::kValue);
    list.rounded_rect({1016.0f, 554.0f, 748.0f, 1.0f}, 0.0f, theme::kRule);
    text_block(c,
               tr("Games use this language when they offer it, and their own default otherwise. It "
               "applies when a game starts."),
               1016.0f, baseline(578.0f, 34.0f, 22.0f), 22.0f, 34.0f, theme::kCopy, 748.0f, 3,
               kShrink);
    if (!message_.empty())
        notice_block(c, message_, 1016.0f, baseline(722.0f, 34.0f, 22.0f), 22.0f, 34.0f,
                     message_warning_ ? theme::kWarning : theme::kLimePale, 748.0f, 2,
                     message_warning_);

    static constexpr Hint kHints[] = {{Pad::cross, TR("Choose")},
                                      {Pad::circle, TR("Back")},
                                      {Pad::updown, TR("Browse")},
                                      {Pad::l1, TR("Page"), Pad::r1}};
    draw_footer(c, kHints, 4);
}

// ---------------------------------------------------------------- about

void Launcher::draw_about(Canvas &c)
{
    gfx::DrawList &list = c.list;
    draw_frame(c, tr("About ProsperoEden"), tr("Credits and setup"));

    glass(c, kListPanel, 26.0f, theme::kPanel.with_alpha(0.80f), theme::kPanelEdge.with_alpha(0.55f));
    text(c, tr("PROJECT CREDITS"), 144.0f, baseline(210.0f, 28.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::left, 3.0f);
    text_shrink(c, tr("Powered by Eden"), 144.0f, baseline(250.0f, 54.0f, theme::kLead),
                theme::kLead, theme::kTitle, 748.0f);
    text_block(c, tr("All credit for the Eden emulator goes to its developers and contributors."),
               144.0f, baseline(310.0f, 36.0f, theme::kText24), theme::kText24, 36.0f, theme::kBody,
               748.0f, 2, kShrink);
    text(c, "eden-emu.dev", 144.0f, baseline(384.0f, 36.0f, theme::kText24), theme::kText24,
         theme::kLimePale);
    list.rounded_rect({144.0f, 444.0f, 748.0f, 1.0f}, 0.0f, theme::kRule);
    text(c, tr("THANKS"), 144.0f, baseline(464.0f, 30.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::left, 2.0f);
    text_block(c,
               tr("Thanks to the whole PS5 homebrew community and to every developer whose drivers, "
               "tools and libraries make ProsperoEden possible."),
               144.0f, baseline(500.0f, 36.0f, theme::kText24), theme::kText24, 36.0f, theme::kBody,
               748.0f, 3, kShrink);
    list.rounded_rect({144.0f, 628.0f, 748.0f, 1.0f}, 0.0f, theme::kRule);
    text(c, tr("PS5 EDITION"), 144.0f, baseline(648.0f, 30.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::left, 2.0f);
    text_block(c, tr("ProsperoEden is an unofficial PS5 port brought to you by BlackBearReloaded."),
               144.0f, baseline(684.0f, 36.0f, theme::kText24), theme::kText24, 36.0f, theme::kBody,
               748.0f, 2, kShrink);
    text_shrink(c, tr("Menu sound effects made with ElevenLabs."), 144.0f,
                baseline(788.0f, 30.0f, theme::kSmall), theme::kSmall, theme::kMeta, 748.0f);
    text(c, version_, 892.0f, baseline(866.0f, 28.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::right);

    glass(c, kDetailPanel, 26.0f, theme::kPanel.with_alpha(0.80f),
          theme::kPanelEdge.with_alpha(0.55f));
    text(c, tr("GETTING STARTED"), 1016.0f, baseline(210.0f, 28.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::left, 3.0f);
    text_shrink(c, tr("Supply your own files"), 1016.0f, baseline(262.0f, 54.0f, theme::kLead),
                theme::kLead, theme::kTitle, 748.0f);
    const std::string folder = services_.files_folder();
    const float column = value_column(
        c, {tr("KEYS"), tr("FIRMWARE"), tr("GAMES"), tr("UPDATES, DLC"), tr("MODS")}, 1016.0f,
        1232.0f, theme::kSmall);
    const auto line = [&](float top, const char *label, const std::string &value)
    {
        text(c, label, 1016.0f, baseline(top, 30.0f, theme::kSmall), theme::kSmall, theme::kMeta);
        text_shrink(c, value, column, baseline(top, 34.0f, theme::kText24), theme::kText24,
                    theme::kValue, 1764.0f - column);
    };
    // Five lines above the rule. A game's own mods folder is named after its ID: its settings in
    // the Library show it (and create it).
    line(384.0f, tr("KEYS"), short_path(folder + "/keys/prod.keys", 36));
    line(450.0f, tr("FIRMWARE"), short_path(folder + "/firmware/*.nca", 36));
    line(516.0f, tr("GAMES"), fill(tr("{0}/ (NSP or XCI)"), {short_path(folder + "/roms", 36)}));
    line(582.0f, tr("UPDATES, DLC"), fill(tr("{0}/ (NSP or XCI)"), {short_path(folder + "/updates", 36)}));
    line(648.0f, tr("MODS"), fill(tr("{0}/ (one folder per game ID)"), {short_path(folder + "/mods", 36)}));
    list.rounded_rect({1016.0f, 712.0f, 748.0f, 1.0f}, 0.0f, theme::kRule);
    text_block(c,
               tr("Use extracted firmware NCA files. Choose the folder in Settings, Game files; "
               "restart the app after changing it."),
               1016.0f, baseline(740.0f, 36.0f, theme::kText24), theme::kText24, 36.0f,
               theme::kMeta, 748.0f, 3, kShrink);

    static constexpr Hint kHints[] = {{Pad::circle, TR("Back")}};
    draw_footer(c, kHints, 1);
}

} // namespace pe::ui
