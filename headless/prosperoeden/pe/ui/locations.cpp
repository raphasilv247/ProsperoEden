// ProsperoEden - Launcher: Settings > Game files: the game files folder, more game folders, and
// where keys, firmware and saves are.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pe/ui/launcher.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace pe::ui
{

using audio::Cue;

namespace
{

// The same fitting of the panels as the folder browser (browse.cpp).
constexpr float kFit = layout::kWidth / 1692.0f;
constexpr float kFitX = layout::kLeft - 108.0f * kFit;
constexpr float kFitY = 152.0f - 188.0f * kFit;

// Placed as in the folder browser (browse.cpp): list on the left, detail on the right.
constexpr Rect kListPanel{108.0f, 188.0f, 820.0f, 720.0f};
constexpr Rect kDetailPanel{980.0f, 188.0f, 820.0f, 720.0f};
constexpr Rect kWindow{138.0f, 292.0f, 760.0f, 518.0f};
constexpr float kRowHeight = 78.0f;

std::string short_path(const std::string &path, std::size_t limit)
{
    return path.size() <= limit ? path : "..." + path.substr(path.size() - (limit - 3));
}

// "3 games": one and many are separate texts, so a language can word each as it needs.
std::string counted(int count, const char *one, const char *many)
{
    return fill(count == 1 ? one : many, {std::to_string(count)});
}

// A folder mark from two rounded shapes (as in the folder browser).
void draw_folder(Canvas &c, float x, float cy, Color ink)
{
    c.list.rounded_rect({x, cy - 13.0f, 18.0f, 8.0f}, 3.0f, ink);
    c.list.rounded_rect({x, cy - 9.0f, 40.0f, 24.0f}, 4.0f, ink);
}

// A plus, as the profiles list has for its new profile.
void draw_plus(Canvas &c, float x, float cy)
{
    const float cx = x + 20.0f;
    c.list.line(cx - 12.0f, cy, cx + 12.0f, cy, 2.8f, th().accent);
    c.list.line(cx, cy - 12.0f, cx, cy + 12.0f, 2.8f, th().accent);
}

} // namespace

void Launcher::read_library_folders()
{
    library_folders_ = services_.library_folders();
}

void Launcher::library_folders_changed()
{
    read_library_folders();
    // The home screen and the Library read the game folders again.
    read_home();
    name_home_games();
    finish_scan(false);
    start_scan();
}

void Launcher::build_locations()
{
    location_list_.clear();
    location_list_.push_back({LocationRow::Kind::main, 0});
    for (int i = 0; i < static_cast<int>(library_folders_.size()); ++i)
        location_list_.push_back({LocationRow::Kind::folder, i});
    if (static_cast<int>(library_folders_.size()) < services_.library_folders_max())
        location_list_.push_back({LocationRow::Kind::add, 0});
    main_used_ = services_.files_folder();
    main_saved_ = services_.saved_files_folder();
}

void Launcher::enter_locations(int select)
{
    read_library_folders();
    build_locations();
    location_rows_.visible = 6;
    location_rows_.pitch = 88.0f;
    const int count = static_cast<int>(location_list_.size());
    location_rows_.reset(count, std::clamp(select, 0, count - 1));
    location_remove_ = -1;
    refresh_location();
}

// What the folder of the focused row holds, read when the focus moves (not every frame).
void Launcher::refresh_location()
{
    location_info_ = {};
    const int row = location_rows_.selected;
    if (row < 0 || row >= static_cast<int>(location_list_.size()))
        return;
    const LocationRow &entry = location_list_[static_cast<std::size_t>(row)];
    std::string folder;
    switch (entry.kind)
    {
    case LocationRow::Kind::main:
        folder = main_used_;
        break;
    case LocationRow::Kind::folder:
        if (entry.index < static_cast<int>(library_folders_.size()))
            folder = library_folders_[static_cast<std::size_t>(entry.index)];
        break;
    default:
        break;
    }
    if (!folder.empty())
        location_info_ = services_.folder_info(folder);
}

void Launcher::press_locations(Key key)
{
    const int count = static_cast<int>(location_list_.size());
    const int row = std::clamp(location_rows_.selected, 0, std::max(count - 1, 0));
    const LocationRow entry = count > 0 ? location_list_[static_cast<std::size_t>(row)] : LocationRow{};
    // Taking a game folder off the list is asked twice; anything else forgets the first press.
    const int armed = location_remove_;
    location_remove_ = -1;
    switch (key)
    {
    case Key::circle:
        open(Screen::settings, false);
        return;
    case Key::up:
    case Key::down:
        if (location_rows_.move(key == Key::down ? 1 : -1))
        {
            message_.clear();
            refresh_location();
            cue(Cue::focus);
        }
        return;
    case Key::l1:
    case Key::r1:
        if (location_rows_.page(key == Key::r1 ? 1 : -1))
        {
            message_.clear();
            refresh_location();
            cue(Cue::page);
        }
        return;
    case Key::cross:
        if (count == 0)
            return;
        browse_from_ = row;
        switch (entry.kind)
        {
        case LocationRow::Kind::main:
            open(Screen::files, true);
            enter_files(Pick::main);
            return;
        case LocationRow::Kind::folder:
            open(Screen::files, true);
            enter_files(Pick::change_folder, entry.index);
            return;
        case LocationRow::Kind::add:
            open(Screen::files, true);
            enter_files(Pick::add_folder);
            return;
        }
        return;
    case Key::square:
        if (count == 0)
            return;
        if (entry.kind == LocationRow::Kind::main)
        {
            const bool saved = services_.set_files_folder(services_.default_files_folder());
            say(saved ? tr("Default folder saved. Reopen ProsperoEden to use it.") :
                        tr("Could not save the folder. Please try again."),
                !saved);
            cue(saved ? Cue::saved : Cue::error);
            if (saved)
            {
                build_locations();
                refresh_location();
            }
            return;
        }
        if (entry.kind == LocationRow::Kind::folder &&
            entry.index < static_cast<int>(library_folders_.size()))
        {
            if (armed != row)
            {
                location_remove_ = row;
                say(fill(tr("Square again takes {0} off the list. Its games stay where they are."),
                         {short_path(library_folders_[static_cast<std::size_t>(entry.index)], 28)}),
                    true);
                cue(Cue::notify);
                return;
            }
            std::vector<std::string> folders = library_folders_;
            folders.erase(folders.begin() + entry.index);
            if (!services_.set_library_folders(folders))
            {
                say(tr("Could not save the folder. Please try again."), true);
                cue(Cue::error);
                return;
            }
            library_folders_changed();
            enter_locations(row);
            say(tr("Taken off the list. Its games stay where they are."));
            cue(Cue::toggle);
        }
        return;
    default:
        return;
    }
}

void Launcher::draw_locations(Canvas &c)
{
    gfx::DrawList &list = c.list;
    const int count = static_cast<int>(location_list_.size());
    draw_frame(c, tr("Game files"), tr("Choose where your games, keys, firmware and saves are"));

    c.list.push_transform(kFit, 0.0f, 0.0f, kFitX, kFitY);

    // ---- the rows ----
    glass(c, kListPanel, th().radius_panel, th().panel.with_alpha(0.80f), th().panel_edge.with_alpha(0.55f));
    text(c, tr("FOLDERS"), 138.0f, baseline(208.0f, 28.0f, th().size_small), th().size_small, th().accent_pale,
         Align::left, 3.0f);
    text(c, counted(static_cast<int>(library_folders_.size()) + 1, tr("{0} game folder"), tr("{0} game folders")),
         138.0f, baseline(246.0f, 28.0f, th().size_small), th().size_small, th().body);
    list.push_clip({kWindow.x - 24.0f, kWindow.y - 6.0f, kWindow.w + 48.0f, kWindow.h + 12.0f});
    const auto row_rect = [&](int row) -> Rect
    {
        return {kWindow.x, kWindow.y + static_cast<float>(row) * location_rows_.pitch - location_rows_.scroll(),
                kWindow.w, kRowHeight};
    };
    for (int row = location_rows_.first_row(); row <= location_rows_.last_row(); ++row)
    {
        list.push_opacity(location_rows_.row_alpha(row, kRowHeight));
        plate_rest(c, list_plate(), row_rect(row));
        list.pop_opacity();
    }
    plate_focus(c, list_plate(),
                {kWindow.x, kWindow.y + location_rows_.cursor() - location_rows_.scroll(), kWindow.w, kRowHeight},
                1.0f);
    for (int row = location_rows_.first_row(); row <= location_rows_.last_row(); ++row)
    {
        const LocationRow &entry = location_list_[static_cast<std::size_t>(row)];
        const Rect r = row_rect(row);
        const float middle = r.y + kRowHeight * 0.5f;
        const float right = r.x + 730.0f;
        list.push_opacity(location_rows_.row_alpha(row, kRowHeight));
        // A label over a path, with a tag at the right end.
        const auto two_lines = [&](float indent, const char *label, const std::string &value, const char *tag,
                                   Color tag_color)
        {
            const float tag_width =
                tag[0] ? text(c, tag, right, baseline(r.y, kRowHeight, th().size_small), th().size_small, tag_color,
                              Align::right)
                       : 0.0f;
            const float room = right - (r.x + 84.0f + indent) - (tag_width > 0.0f ? tag_width + 24.0f : 0.0f);
            text_fit(c, label, r.x + 84.0f + indent, baseline(r.y + 6.0f, 30.0f, th().size_small), th().size_small,
                     th().label, room);
            text_fit(c, short_path(value, 46), r.x + 84.0f + indent, baseline(r.y + 38.0f, 34.0f, th().size_text),
                     th().size_text, th().value, room);
        };
        switch (entry.kind)
        {
        case LocationRow::Kind::main:
            draw_folder(c, r.x + 24.0f, middle, th().accent_pale);
            two_lines(0.0f, tr("GAME FILES FOLDER"), main_saved_.empty() ? main_used_ : main_saved_,
                      !main_saved_.empty() && main_saved_ != main_used_ ? tr("NEXT START") : "", th().meta);
            break;
        case LocationRow::Kind::folder:
            draw_folder(c, r.x + 24.0f, middle, th().copy);
            two_lines(0.0f, tr("GAME FOLDER"), library_folders_[static_cast<std::size_t>(entry.index)], "",
                      th().meta);
            break;
        case LocationRow::Kind::add:
            draw_plus(c, r.x + 24.0f, middle);
            text_fit(c, tr("Add game folder"), r.x + 84.0f, baseline(r.y, kRowHeight, th().size_text),
                     th().size_text, th().value, 646.0f);
            break;
        }
        list.pop_opacity();
    }
    list.pop_clip();
    scrollbar(c, location_rows_, 910.0f, kWindow.y, kWindow.h);
    text(c, list_position(count > 0 ? location_rows_.selected + 1 : 0, count), 898.0f,
         baseline(866.0f, 28.0f, th().size_small), th().size_small, th().accent_pale, Align::right);

    // ---- what the focused row is, and what its folder holds ----
    glass(c, kDetailPanel, th().radius_panel, th().panel.with_alpha(0.80f), th().panel_edge.with_alpha(0.55f));
    const LocationRow focus = count > 0 ? location_list_[static_cast<std::size_t>(
                                              std::clamp(location_rows_.selected, 0, count - 1))]
                                        : LocationRow{};
    const char *label = tr("THIS FOLDER");
    std::string heading;
    const char *about = "";
    switch (focus.kind)
    {
    case LocationRow::Kind::main:
        heading = main_used_;
        about = tr("Holds keys, firmware, roms, updates, mods and saves, unless you choose some of them on their "
                   "own (Advanced).");
        break;
    case LocationRow::Kind::folder:
        heading = focus.index < static_cast<int>(library_folders_.size()) ?
                      library_folders_[static_cast<std::size_t>(focus.index)] :
                      std::string{};
        about = tr("Games, updates and mods are read from this folder. Press Square twice to take it off the list.");
        break;
    case LocationRow::Kind::add:
        label = tr("GAME FOLDERS");
        heading = tr("Add game folder");
        about = tr("Add other game folders, with roms, updates and mods on an external HD or SSD over USB, "
                   "or in the console's extended storage.");
        break;
    }
    text(c, label, 1016.0f, baseline(210.0f, 28.0f, th().size_small), th().size_small, th().accent_pale, Align::left,
         3.0f);
    text_block(c, heading, 1016.0f, baseline(250.0f, 42.0f, th().size_heading), th().size_heading, 42.0f, th().title,
               748.0f, 2);
    const float column = value_column(
        c, {tr("KEYS"), tr("FIRMWARE"), tr("GAMES"), tr("UPDATES, DLC"), tr("IN USE")}, 1016.0f, 1216.0f,
        th().size_small, 2.0f);
    // A line whose value is in the warning colour carries the warning mark too.
    const auto line = [&](float top, const char *name, const std::string &value, Color color)
    {
        text(c, name, 1016.0f, baseline(top, 30.0f, th().size_small), th().size_small, th().label, Align::left, 2.0f);
        notice(c, value, column, baseline(top, 30.0f, th().size_small), th().size_small, color, 1764.0f - column,
               color.r == th().warning.r && color.g == th().warning.g);
    };
    const auto state = [](bool ready) { return ready ? th().accent_pale : th().warning; };
    const FolderInfo &info = location_info_;
    // Games and updates of the game files folder and of a game folder; -1 where a subfolder is missing.
    const auto games_line = [&](float top)
    {
        line(top, tr("GAMES"),
             info.games < 0 ? tr("No roms folder") : counted(info.games, tr("{0} game"), tr("{0} games")),
             state(info.games > 0));
    };
    const auto updates_line = [&](float top)
    {
        line(top, tr("UPDATES, DLC"),
             info.updates < 0 ? tr("No updates folder") : counted(info.updates, tr("{0} file"), tr("{0} files")),
             th().value);
    };
    const auto in_use_line = [&](float top, const std::string &used, const std::string &saved)
    {
        line(top, tr("IN USE"),
             !saved.empty() && saved != used ? fill(tr("Next launch: {0}"), {short_path(saved, 32)}) :
                                               short_path(used, 40),
             th().value);
    };
    switch (focus.kind)
    {
    case LocationRow::Kind::add:
        text(c, tr("PATH TIPS"), 1016.0f, baseline(378.0f, 30.0f, th().size_small), th().size_small, th().label,
             Align::left, 2.0f);
        line(430.0f, tr("EXTENDED STORAGE"), "/extX/prosperoeden/", th().accent_pale);
        line(482.0f, tr("EXTERNAL HD OR SSD"), "/usbX/prosperoeden/", th().accent_pale);
        break;
    case LocationRow::Kind::main:
        line(378.0f, tr("KEYS"), info.keys ? tr("prod.keys found") : tr("prod.keys missing"), state(info.keys));
        line(430.0f, tr("FIRMWARE"),
             info.firmware < 0 ? tr("No firmware folder") :
                                 counted(info.firmware, tr("{0} NCA file"), tr("{0} NCA files")),
             state(info.firmware > 0));
        games_line(482.0f);
        updates_line(534.0f);
        in_use_line(586.0f, main_used_, main_saved_);
        break;
    case LocationRow::Kind::folder:
        games_line(378.0f);
        updates_line(430.0f);
        break;
    default:
        break;
    }
    list.rounded_rect({1016.0f, 640.0f, 748.0f, 1.0f}, 0.0f, th().rule);
    notice_block(c, message_.empty() ? std::string{about} : message_, 1016.0f,
                 baseline(662.0f, 36.0f, th().size_text), th().size_text, 36.0f,
                 message_.empty() ? th().copy : message_warning_ ? th().warning : th().accent_pale, 748.0f, 5,
                 !message_.empty() && message_warning_);

    c.list.pop_transform();
    switch (focus.kind)
    {
    case LocationRow::Kind::main:
    {
        static constexpr Hint kHints[] = {{Pad::cross, TR("Open")},
                                          {Pad::circle, TR("Back")},
                                          {Pad::square, TR("Default folder")},
                                          {Pad::updown, TR("Browse")},
                                          {Pad::l1, TR("Page"), Pad::r1}};
        draw_footer(c, kHints, 5);
        break;
    }
    case LocationRow::Kind::folder:
    {
        static constexpr Hint kHints[] = {{Pad::cross, TR("Change")},
                                          {Pad::circle, TR("Back")},
                                          {Pad::square, TR("Remove")},
                                          {Pad::updown, TR("Browse")},
                                          {Pad::l1, TR("Page"), Pad::r1}};
        draw_footer(c, kHints, 5);
        break;
    }
    case LocationRow::Kind::add:
    {
        static constexpr Hint kHints[] = {{Pad::cross, TR("Add")},
                                          {Pad::circle, TR("Back")},
                                          {Pad::updown, TR("Browse")},
                                          {Pad::l1, TR("Page"), Pad::r1}};
        draw_footer(c, kHints, 4);
        break;
    }
    }
}

} // namespace pe::ui
