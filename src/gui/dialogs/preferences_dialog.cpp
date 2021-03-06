/*
   Copyright (C) 2011, 2015 by Ignacio R. Morelle <shadowm2006@gmail.com>
   Copyright (C) 2016 by Charles Dang <exodia339gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/preferences_dialog.hpp"

#include "config_assign.hpp"
#include "gettext.hpp"
#include "filesystem.hpp"
#include "formatter.hpp"
#include "formula/string_utils.hpp"
#include "game_preferences.hpp"
#include "hotkey/hotkey_command.hpp"
#include "hotkey/hotkey_item.hpp"
#include "lobby_preferences.hpp"
#include "preferences.hpp"
#include "preferences_display.hpp"
#include "video.hpp"

// Sub-dialog includes
#include "gui/dialogs/advanced_graphics_options.hpp"
#include "gui/dialogs/game_cache_options.hpp"
#include "gui/dialogs/logging.hpp"
#include "gui/dialogs/multiplayer/mp_alerts_options.hpp"
#include "gui/dialogs/select_orb_colors.hpp"

#include "gui/auxiliary/find_widget.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/transient_message.hpp"
#include "gui/widgets/button.hpp"
#include "gui/widgets/menu_button.hpp"
#include "gui/widgets/grid.hpp"
#include "gui/widgets/image.hpp"
#include "gui/widgets/label.hpp"
#ifdef GUI2_EXPERIMENTAL_LISTBOX
#include "gui/widgets/list.hpp"
#else
#include "gui/widgets/listbox.hpp"
#endif
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/slider.hpp"
#include "gui/widgets/stacked_widget.hpp"
#include "gui/widgets/status_label_helper.hpp"
#include "gui/widgets/text_box.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/window.hpp"
#include "util.hpp"

#include "utils/functional.hpp"
#include <boost/math/common_factor_rt.hpp>

namespace gui2 {

using namespace preferences;

REGISTER_DIALOG(preferences)

tpreferences::tpreferences(CVideo& video, const config& game_cfg, const PREFERENCE_VIEW& initial_view)
	: resolutions_(video.get_available_resolutions(true))
	, adv_preferences_cfg_()
	, last_selected_item_(0)
	, accl_speeds_(
		// IMPORTANT: NEVER have trailing zeroes here, or else the cast from doubles
		// to string will not match, since lexical_cast strips trailing zeroes.
		{"0.25", "0.5", "0.75", "1", "1.25", "1.5", "1.75", "2", "3", "4", "8", "16" })
	, visible_hotkeys_()
	, initial_index_(pef_view_map[initial_view])
{
	for(const config& adv : game_cfg.child_range("advanced_preference")) {
		adv_preferences_cfg_.push_back(adv);
	}

	std::sort(adv_preferences_cfg_.begin(), adv_preferences_cfg_.end(),
		[](const config& lhs, const config& rhs) {
			return lhs["name"].t_str().str() < rhs["name"].t_str().str();
		});
}

// Helper function to refresh resolution list
void tpreferences::set_resolution_list(tmenu_button& res_list, CVideo& video)
{
	resolutions_ = video.get_available_resolutions(true);

	std::vector<config> options;
	for(const auto& res : resolutions_)
	{
		config option;
		option["label"] = formatter() << res.first << font::unicode_multiplication_sign << res.second;

		const int div = boost::math::gcd(res.first, res.second);
		const int ratio[2] = {res.first/div, res.second/div};
		if(ratio[0] <= 10 || ratio[1] <= 10) {
			option["details"] = formatter() << "<span color='#777777'>(" << ratio[0] << ':' << ratio[1] << ")</span>";
		}

		options.push_back(option);
	}

	const unsigned current_res = std::find(resolutions_.begin(), resolutions_.end(),
		video.current_resolution()) - resolutions_.begin();

	res_list.set_values(options, current_res);
}

std::map<std::string, string_map> tpreferences::get_friends_list_row_data(const acquaintance& entry)
{
	std::map<std::string, string_map> data;
	string_map item;

	std::string image = "friend.png";
	std::string descriptor = _("friend");
	std::string notes;

	if(entry.get_status() == "ignore") {
		image = "ignore.png";
		descriptor = _("ignored");
	}

	if(!entry.get_notes().empty()) {
		notes = " <small>(" + entry.get_notes() + ")</small>";
	}

	item["use_markup"] = "true";

	item["label"] = "misc/status-" + image;
	data.emplace("friend_icon", item);

	item["label"] = entry.get_nick() + notes;
	data.emplace("friend_name", item);

	item["label"] = "<small>" + descriptor + "</small>";
	data.emplace("friend_status", item);

	return data;
}

void tpreferences::on_friends_list_select(tlistbox& list, ttext_box& textbox)
{
	const int num_friends = get_acquaintances().size();
	const int sel = list.get_selected_row();

	if(sel < 0 || sel >= num_friends) {
		return;
	}

	std::map<std::string, acquaintance>::const_iterator who = get_acquaintances().begin();
	std::advance(who, sel);

	textbox.set_value(who->second.get_nick() + " " + who->second.get_notes());
}

void tpreferences::update_friends_list_controls(twindow& window, tlistbox& list)
{
	const bool list_empty = list.get_item_count() == 0;

	if(!list_empty) {
		list.select_row(std::min(static_cast<int>(list.get_item_count()) - 1, list.get_selected_row()));
	}

	find_widget<tbutton>(&window, "remove", false).set_active(!list_empty);

	find_widget<tlabel>(&window, "no_friends_notice", false).set_visible(
		list_empty ? twindow::tvisible::visible : twindow::tvisible::invisible);
}

void tpreferences::add_friend_list_entry(const bool is_friend, ttext_box& textbox, twindow& window)
{
	std::string username = textbox.text();
	if(username.empty()) {
		gui2::show_transient_message(window.video(), "", _("No username specified"), "", false, false, true);
		return;
	}

	std::string reason;

	size_t pos = username.find_first_of(' ');
	if(pos != std::string::npos) {
		reason = username.substr(pos + 1);
		username = username.substr(0, pos);
	}

	acquaintance* entry = add_acquaintance(username, (is_friend ? "friend": "ignore"), reason);
	if(!entry) {
		gui2::show_transient_message(window.video(), _("Error"), _("Invalid username"), "", false, false, true);
		return;
	}

	textbox.clear();

	tlistbox& list = find_widget<tlistbox>(&window, "friends_list", false);
	list.add_row(get_friends_list_row_data(*entry));

	update_friends_list_controls(window, list);
}

void tpreferences::remove_friend_list_entry(tlistbox& friends_list, ttext_box& textbox, twindow& window)
{
	const int selected_row = std::max(0, friends_list.get_selected_row());

	std::map<std::string, acquaintance>::const_iterator who = get_acquaintances().begin();
	std::advance(who, selected_row);

	const std::string to_remove = !textbox.text().empty() ? textbox.text() : who->second.get_nick();

	if(to_remove.empty()) {
		gui2::show_transient_message(window.video(), "", _("No username specified"), "", false, false, true);
		return;
	}

	if(!remove_acquaintance(to_remove)) {
		gui2::show_transient_error_message(window.video(), _("Not on friends or ignore lists"));
		return;
	}

	textbox.clear();

	tlistbox& list = find_widget<tlistbox>(&window, "friends_list", false);
	list.remove_row(selected_row);

	update_friends_list_controls(window, list);
}

// Helper function to get the main grid in each row of the advanced section
// lisbox, which contains the value and setter widgets.
static tgrid* get_advanced_row_grid(tlistbox& list, const int selected_row)
{
	return dynamic_cast<tgrid*>(list.get_row_grid(selected_row)->find("pref_main_grid", false));
}

template<typename W>
static void disable_widget_on_toggle(twindow& window, twidget& w, const std::string& id)
{
	find_widget<W>(&window, id, false).set_active(dynamic_cast<tselectable_&>(w).get_value_bool());
};

/**
 * Sets up states and callbacks for each of the widgets
 */
void tpreferences::post_build(twindow& window)
{
	//
	// GENERAL PANEL
	//

	/* SCROLL SPEED */
	register_integer("scroll_speed", true,
		scroll_speed, set_scroll_speed);

	/* ACCELERATED SPEED */
	register_bool("turbo_toggle", true, turbo, set_turbo,
		[&](twidget& w) { disable_widget_on_toggle<tslider>(window, w, "turbo_slider"); }, true);

	const auto accl_load = [this]()->int {
		return std::find(accl_speeds_.begin(), accl_speeds_.end(),
			lexical_cast<std::string>(turbo_speed())) - accl_speeds_.begin() + 1;
	};

	const auto accl_save = [this](int i) {
		set_turbo_speed(lexical_cast<double>(accl_speeds_[i - 1]));
	};

	register_integer("turbo_slider", true,
		accl_load, accl_save);

	// Manually set the value labels
	find_widget<tslider>(&window, "turbo_slider", false).set_value_labels(accl_speeds_);

	/* SKIP AI MOVES */
	register_bool("skip_ai_moves", true,
		skip_ai_moves, set_skip_ai_moves);

	/* DISABLE AUTO MOVES */
	register_bool("disable_auto_moves", true,
		disable_auto_moves, set_disable_auto_moves);

	/* TURN DIALOG */
	register_bool("show_turn_dialog", true,
		turn_dialog, set_turn_dialog);

	/* ENABLE PLANNING MODE */
	register_bool("whiteboard_on_start", true,
		enable_whiteboard_mode_on_start, set_enable_whiteboard_mode_on_start);

	/* HIDE ALLY PLANS */
	register_bool("whiteboard_hide_allies", true,
		hide_whiteboard, set_hide_whiteboard);

	/* INTERRUPT ON SIGHTING */
	register_bool("interrupt_move_when_ally_sighted", true,
		interrupt_when_ally_sighted, set_interrupt_when_ally_sighted);

	/* SAVE REPLAYS */
	register_bool("save_replays", true,
		save_replays, set_save_replays);

	/* DELETE AUTOSAVES */
	register_bool("delete_saves", true,
		delete_saves, set_delete_saves);

	/* MAX AUTO SAVES */
	register_integer("max_saves_slider", true,
		autosavemax, set_autosavemax);

	/* CACHE MANAGE */
	connect_signal_mouse_left_click(find_widget<tbutton>(&window, "cachemg", false),
			std::bind(&gui2::tgame_cache_options::display,
			std::ref(window.video())));

	//
	// DISPLAY PANEL
	//

	/* FULLSCREEN TOGGLE */
	ttoggle_button& toggle_fullscreen =
			find_widget<ttoggle_button>(&window, "fullscreen", false);

	toggle_fullscreen.set_value(fullscreen());

	// We bind a special callback function, so setup_single_toggle() is not used
	connect_signal_mouse_left_click(toggle_fullscreen, std::bind(
			&tpreferences::fullscreen_toggle_callback,
			this, std::ref(window)));

	/* SET RESOLUTION */
	tmenu_button& res_list = find_widget<tmenu_button>(&window, "resolution_set", false);

	res_list.set_use_markup(true);
	res_list.set_active(!fullscreen());

	set_resolution_list(res_list, window.video());

	res_list.connect_click_handler(
			std::bind(&tpreferences::handle_res_select,
			this, std::ref(window)));

	/* SHOW FLOATING LABELS */
	register_bool("show_floating_labels", true,
		show_floating_labels, set_show_floating_labels);

	/* SHOW TEAM COLORS */
	register_bool("show_ellipses", true,
		show_side_colors, set_show_side_colors);

	/* SHOW GRID */
	register_bool("show_grid", true,
		grid, set_grid);

	/* ANIMATE MAP */
	register_bool("animate_terrains", true, animate_map, set_animate_map,
		[&](twidget& w) { disable_widget_on_toggle<ttoggle_button>(window, w, "animate_water"); }, true);

	/* ANIMATE WATER */
	register_bool("animate_water", true,
		animate_water, set_animate_water);

	/* SHOW UNIT STANDING ANIMS */
	register_bool("animate_units_standing", true,
		show_standing_animations, set_show_standing_animations);

	/* SHOW UNIT IDLE ANIMS */
	register_bool("animate_units_idle", true, idle_anim, set_idle_anim,
		[&](twidget& w) { disable_widget_on_toggle<tslider>(window, w, "idle_anim_frequency"); });

	register_integer("idle_anim_frequency", true,
		idle_anim_rate, set_idle_anim_rate);

	/* FONT SCALING */
	register_integer("scaling_slider", true,
		font_scaling, set_font_scaling);

	/* SELECT THEME */
	connect_signal_mouse_left_click(
			find_widget<tbutton>(&window, "choose_theme", false),
			bind_void(&show_theme_dialog,
			std::ref(window.video())));


	//
	// SOUND PANEL
	//

	/* SOUND FX */
	register_bool("sound_toggle_sfx", true, sound_on, bind_void(set_sound, _1),
		[&](twidget& w) { disable_widget_on_toggle<tslider>(window, w, "sound_volume_sfx"); }, true);

	register_integer("sound_volume_sfx", true,
		sound_volume, set_sound_volume);

	/* MUSIC */
	register_bool("sound_toggle_music", true, music_on, bind_void(set_music, _1),
		[&](twidget& w) { disable_widget_on_toggle<tslider>(window, w, "sound_volume_music"); }, true);

	register_integer("sound_volume_music", true,
		music_volume, set_music_volume);

	register_bool("sound_toggle_stop_music_in_background", true,
		stop_music_in_background, set_stop_music_in_background);

	/* TURN BELL */
	register_bool("sound_toggle_bell", true, turn_bell, bind_void(set_turn_bell, _1),
		[&](twidget& w) { disable_widget_on_toggle<tslider>(window, w, "sound_volume_bell"); }, true);

	register_integer("sound_volume_bell", true,
		bell_volume, set_bell_volume);

	/* UI FX */
	register_bool("sound_toggle_uisfx", true, UI_sound_on, bind_void(set_UI_sound, _1),
		[&](twidget& w) { disable_widget_on_toggle<tslider>(window, w, "sound_volume_uisfx"); }, true);

	register_integer("sound_volume_uisfx", true,
		UI_volume, set_UI_volume);


	//
	// MULTIPLAYER PANEL
	//

	/* CHAT LINES */
	register_integer("chat_lines", true,
		chat_lines, set_chat_lines);

	/* CHAT TIMESTAMPPING */
	register_bool("chat_timestamps", true,
		chat_timestamping, set_chat_timestamping);

	/* SAVE PASSWORD */
	register_bool("remember_password", true,
		remember_password, set_remember_password);

	/* SORT LOBBY LIST */
	register_bool("lobby_sort_players", true,
		sort_list, _set_sort_list);

	/* ICONIZE LOBBY LIST */
	register_bool("lobby_player_icons", true,
		iconize_list, _set_iconize_list);

	/* WHISPERS FROM FRIENDS ONLY */
	register_bool("lobby_whisper_friends_only", true,
		whisper_friends_only, set_whisper_friends_only);

	/* LOBBY JOIN NOTIFICATIONS */
	lobby_joins_group.add_member(&find_widget<ttoggle_button>(&window, "lobby_joins_none", false), SHOW_NONE);
	lobby_joins_group.add_member(&find_widget<ttoggle_button>(&window, "lobby_joins_friends", false), SHOW_FRIENDS);
	lobby_joins_group.add_member(&find_widget<ttoggle_button>(&window, "lobby_joins_all", false), SHOW_ALL);

	lobby_joins_group.set_member_states(static_cast<LOBBY_JOINS>(lobby_joins()));

	lobby_joins_group.set_callback_on_value_change([&](twidget&) {
		_set_lobby_joins(lobby_joins_group.get_active_member_value());
	});

	/* FRIENDS LIST */
	tlistbox& friends_list = find_widget<tlistbox>(&window, "friends_list", false);

	friends_list.clear();

	for(const auto& entry : get_acquaintances()) {
		friends_list.add_row(get_friends_list_row_data(entry.second));
	}

	update_friends_list_controls(window, friends_list);

	ttext_box& textbox = find_widget<ttext_box>(&window, "friend_name_box", false);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "add_friend", false), std::bind(
			&tpreferences::add_friend_list_entry,
			this, true,
			std::ref(textbox),
			std::ref(window)));

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "add_ignored", false), std::bind(
			&tpreferences::add_friend_list_entry,
			this, false,
			std::ref(textbox),
			std::ref(window)));

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "remove", false), std::bind(
			&tpreferences::remove_friend_list_entry,
			this,
			std::ref(friends_list),
			std::ref(textbox),
			std::ref(window)));

	friends_list.set_callback_value_change(std::bind(
			&tpreferences::on_friends_list_select,
			this,
			std::ref(friends_list),
			std::ref(textbox)));

	/* ALERTS */
	connect_signal_mouse_left_click(
			find_widget<tbutton>(&window, "mp_alerts", false),
			std::bind(&gui2::tmp_alerts_options::display,
			std::ref(window.video())));

	/* SET WESNOTHD PATH */
	connect_signal_mouse_left_click(
			find_widget<tbutton>(&window, "mp_wesnothd", false), std::bind(
			&show_wesnothd_server_search,
			std::ref(window.video())));


	//
	// ADVANCED PANEL
	//

	tlistbox& advanced = find_widget<tlistbox>(&window, "advanced_prefs", false);

	std::map<std::string, string_map> row_data;

	for(const config& option : adv_preferences_cfg_) {
		// Details about the current option
		ADVANCED_PREF_TYPE pref_type;
		try {
			pref_type = ADVANCED_PREF_TYPE::string_to_enum(option["type"].str());
		} catch(bad_enum_cast&) {
			continue;
		}

		const std::string& pref_name = option["field"].str();

		row_data["pref_name"]["label"] = option["name"];
		advanced.add_row(row_data);

		const int this_row = advanced.get_item_count() - 1;

		// Get the main grid from each row
		tgrid* main_grid = get_advanced_row_grid(advanced, this_row);
		assert(main_grid);

		tgrid& details_grid = find_widget<tgrid>(main_grid, "prefs_setter_grid", false);
		details_grid.set_visible(tcontrol::tvisible::invisible);

		// The toggle widget for toggle-type options (hidden for other types)
		ttoggle_button& toggle_box = find_widget<ttoggle_button>(main_grid, "value_toggle", false);
		toggle_box.set_visible(tcontrol::tvisible::hidden);

		if(!option["description"].empty()) {
			find_widget<tcontrol>(main_grid, "description", false).set_label(option["description"]);
		}

		switch(pref_type.v) {
			case ADVANCED_PREF_TYPE::TOGGLE: {
				//main_grid->remove_child("setter");

				toggle_box.set_visible(tcontrol::tvisible::visible);
				toggle_box.set_value(get(pref_name, option["default"].to_bool()));

				connect_signal_mouse_left_click(toggle_box, std::bind(
					[&, pref_name]() { set(pref_name, toggle_box.get_value_bool()); }
				));

				gui2::bind_status_label<ttoggle_button>(*main_grid, "value_toggle", [](ttoggle_button& t)->std::string {
					return t.get_value_bool() ? _("yes") : _("no");
				}, "value");

				break;
			}

			case ADVANCED_PREF_TYPE::SLIDER: {
				tslider* setter_widget = new tslider;
				setter_widget->set_definition("minimal");
				setter_widget->set_id("setter");
				// Maximum must be set first or this will assert
				setter_widget->set_maximum_value(option["max"].to_int());
				setter_widget->set_minimum_value(option["min"].to_int());
				setter_widget->set_step_size(
					option["step"].empty() ? 1 : option["step"].to_int());

				delete details_grid.swap_child("setter", setter_widget, true);

				tslider& slider = find_widget<tslider>(&details_grid, "setter", false);

				slider.set_value(lexical_cast_default<int>(get(pref_name), option["default"].to_int()));

				connect_signal_notify_modified(slider, std::bind(
					[&, pref_name]() { set(pref_name, slider.get_value()); }
				));

				gui2::bind_status_label<tslider>(*main_grid, "setter", [](tslider& s)->std::string {
					return std::to_string(s.get_value());
				}, "value");

				break;
			}

			case ADVANCED_PREF_TYPE::COMBO: {
				std::vector<config> menu_data;
				std::vector<std::string> option_ids;

				for(const config& choice : option.child_range("option")) {
					config menu_item;
					menu_item["label"] = choice["name"];
					if(choice.has_attribute("description")) {
						menu_item["details"] = std::string("<span color='#777'>") + choice["description"] + "</span>";
					}
					menu_data.push_back(menu_item);
					option_ids.push_back(choice["id"]);
				}

				const unsigned selected = std::find(option_ids.begin(), option_ids.end(),
					get(pref_name, option["default"].str())) - option_ids.begin();

				tmenu_button* setter_widget = new tmenu_button;
				setter_widget->set_definition("default");
				setter_widget->set_id("setter");

				delete details_grid.swap_child("setter", setter_widget, true);

				tmenu_button& menu = find_widget<tmenu_button>(&details_grid, "setter", false);

				menu.set_use_markup(true);
				menu.set_values(menu_data, selected);
				menu.set_callback_state_change([=](twidget& w) {
					set(pref_name, option_ids[dynamic_cast<tmenu_button&>(w).get_value()]);
				});

				gui2::bind_status_label<tmenu_button>(*main_grid, "setter", [](tmenu_button& m)->std::string {
					return m.get_value_string();
				}, "value");

				break;
			}

			case ADVANCED_PREF_TYPE::SPECIAL: {
				//main_grid->remove_child("setter");

				timage* value_widget = new timage;
				value_widget->set_definition("default");
				value_widget->set_label("icons/arrows/arrows_blank_right_25.png~CROP(3,3,18,18)");

				delete main_grid->swap_child("value", value_widget, true);

				break;
			}
		}
	}

#ifdef GUI2_EXPERIMENTAL_LISTBOX
	connect_signal_notify_modified(advanced, std::bind(
		&tpreferences::on_advanced_prefs_list_select,
		this,
		std::ref(advanced),
		std::ref(window)));
#else
	advanced.set_callback_value_change([&, this](twidget&) {
		on_advanced_prefs_list_select(advanced, window);
	});
#endif

	on_advanced_prefs_list_select(advanced, window);

	//
	// HOTKEYS PANEL
	//

	row_data.clear();

	tlistbox& hotkey_categories = find_widget<tlistbox>(&window, "list_categories", false);

	const std::string cat_names[] = {
		// TODO: This list needs to be synchronized with the hotkey::HOTKEY_CATEGORY enum
		// Find some way to do that automatically
		_("General"),
		_("Saved Games"),
		_("Map Commands"),
		_("Unit Commands"),
		_("Player Chat"),
		_("Replay Control"),
		_("Planning Mode"),
		_("Scenario Editor"),
		_("Editor Palettes"),
		_("Editor Tools"),
		_("Editor Clipboard"),
		_("Debug Commands"),
		_("Custom WML Commands"),
		// HKCAT_PLACEHOLDER intentionally excluded (it shouldn't have any anyway)
	};

	for(int i = 0; i < hotkey::HKCAT_PLACEHOLDER; i++) {
		row_data["cat_label"]["label"] = cat_names[i];
		hotkey_categories.add_row(row_data);
		hotkey_categories.select_row(hotkey_categories.get_item_count() - 1);
	}

	setup_hotkey_list(window);

	tlistbox& hotkey_list = find_widget<tlistbox>(&window, "list_hotkeys", false);

	hotkey_categories.set_callback_item_change([this, &hotkey_list, &hotkey_categories](size_t i) {
		if(i >= hotkey::HKCAT_PLACEHOLDER) {
			return;
		}

		hotkey::HOTKEY_CATEGORY cat = hotkey::HOTKEY_CATEGORY(i);

		// For listboxes that allow multiple selection, get_selected_row() returns the most
		// recently selected row. Thus, if it returns i, this row was just selected.
		// Otherwise, it must have been deselected.
		bool show = hotkey_categories.get_selected_row() == int(i);
		boost::dynamic_bitset<> mask = hotkey_list.get_rows_shown();
		for(size_t j = 0; j < visible_hotkeys_.size(); j++) {
			if(visible_hotkeys_[j]->category == cat) {
				mask[j] = show;
			}
		}
		hotkey_list.set_row_shown(mask);
	});

	// Action column
	hotkey_list.register_sorting_option(0, [this](const int i) { return visible_hotkeys_[i]->description.str(); });

	// Hotkey column
	hotkey_list.register_sorting_option(1, [this](const int i) { return hotkey::get_names(visible_hotkeys_[i]->command); });

	// Scope columns
	hotkey_list.register_sorting_option(2, [this](const int i) { return !visible_hotkeys_[i]->scope[hotkey::SCOPE_GAME]; });
	hotkey_list.register_sorting_option(3, [this](const int i) { return !visible_hotkeys_[i]->scope[hotkey::SCOPE_EDITOR]; });
	hotkey_list.register_sorting_option(4, [this](const int i) { return !visible_hotkeys_[i]->scope[hotkey::SCOPE_MAIN_MENU]; });

	hotkey_list.set_active_sorting_option({0, tlistbox::SORT_ASCENDING}, true);

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "btn_add_hotkey", false), std::bind(
			&tpreferences::add_hotkey_callback,
			this,
			std::ref(hotkey_list)));

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "btn_clear_hotkey", false), std::bind(
			&tpreferences::remove_hotkey_callback,
			this,
			std::ref(hotkey_list)));

	connect_signal_mouse_left_click(
		find_widget<tbutton>(&window, "btn_reset_hotkeys", false), std::bind(
			&tpreferences::default_hotkey_callback,
			this,
			std::ref(window)));
}

void tpreferences::setup_hotkey_list(twindow& window)
{
	const std::string& default_icon = "misc/empty.png~CROP(0,0,15,15)";

	std::map<std::string, string_map> row_data;

	t_string& row_icon =   row_data["img_icon"]["label"];
	t_string& row_action = row_data["lbl_desc"]["label"];
	t_string& row_hotkey = row_data["lbl_hotkey"]["label"];

	t_string& row_is_g        = row_data["lbl_is_game"]["label"];
	t_string& row_is_g_markup = row_data["lbl_is_game"]["use_markup"];
	t_string& row_is_e        = row_data["lbl_is_editor"]["label"];
	t_string& row_is_e_markup = row_data["lbl_is_editor"]["use_markup"];
	t_string& row_is_t        = row_data["lbl_is_titlescreen"]["label"];
	t_string& row_is_t_markup = row_data["lbl_is_titlescreen"]["use_markup"];

	tlistbox& hotkey_list = find_widget<tlistbox>(&window, "list_hotkeys", false);

	hotkey_list.clear();
	visible_hotkeys_.clear();

	std::string text_feature_on =  "<span color='#0f0'>" + _("&#9679;") + "</span>";

	for(const auto& hotkey_item : hotkey::get_hotkey_commands()) {
		if(hotkey_item.hidden) {
			continue;
		}
		visible_hotkeys_.push_back(&hotkey_item);

		if(filesystem::file_exists(game_config::path + "/images/icons/action/" + hotkey_item.command + "_25.png")) {
			row_icon = "icons/action/" + hotkey_item.command + "_25.png~CROP(3,3,18,18)";
		} else {
			row_icon = default_icon;
		}

		row_action = hotkey_item.description;
		row_hotkey = hotkey::get_names(hotkey_item.command);

		row_is_g = hotkey_item.scope[hotkey::SCOPE_GAME]      ? text_feature_on : "";
		row_is_g_markup = "true";
		row_is_e = hotkey_item.scope[hotkey::SCOPE_EDITOR]    ? text_feature_on : "";
		row_is_e_markup = "true";
		row_is_t = hotkey_item.scope[hotkey::SCOPE_MAIN_MENU] ? text_feature_on : "";
		row_is_t_markup = "true";

		hotkey_list.add_row(row_data);
	}
}

void tpreferences::add_hotkey_callback(tlistbox& hotkeys)
{
	CVideo& video = hotkeys.get_window()->video();
	int row_number = hotkeys.get_selected_row();
	const hotkey::hotkey_command& hotkey_item = *visible_hotkeys_[row_number];
	hotkey::hotkey_ptr newhk = hotkey::show_binding_dialog(video, hotkey_item.command);
	hotkey::hotkey_ptr oldhk;

	// only if not cancelled.
	if(newhk.get() == nullptr) {
		return;
	}

	for(const hotkey::hotkey_ptr& hk : hotkey::get_hotkeys()) {
		if(!hk->is_disabled() && newhk->bindings_equal(hk)) {
			oldhk = hk;
		}
	}

	hotkey::scope_changer scope_restorer;
	hotkey::set_active_scopes(hotkey_item.scope);

	if(oldhk && oldhk->get_command() == hotkey_item.command) {
		return;
	}

	if(oldhk) {
		const std::string text = vgettext("“<b>$hotkey_sequence|</b>” is in use by “<b>$old_hotkey_action|</b>”.\nDo you wish to reassign it to “<b>$new_hotkey_action|</b>”?", {
			{"hotkey_sequence",   oldhk->get_name()},
			{"old_hotkey_action", hotkey::get_description(oldhk->get_command())},
			{"new_hotkey_action", hotkey::get_description(newhk->get_command())}
		});

		const int res = gui2::show_message(video, _("Reassign Hotkey"), text, gui2::tmessage::yes_no_buttons, true);
		if(res != gui2::twindow::OK) {
			return;
		}
	}

	hotkey::add_hotkey(newhk);

	// We need to recalculate all hotkey names in because we might have removed a hotkey from another command.
	for(size_t i = 0; i < hotkeys.get_item_count(); ++i) {
		const hotkey::hotkey_command& hotkey_item_row = *visible_hotkeys_[i];
		find_widget<tlabel>(hotkeys.get_row_grid(i), "lbl_hotkey", false).set_label(hotkey::get_names(hotkey_item_row.command));
	}
}

void tpreferences::default_hotkey_callback(twindow& window)
{
	gui2::show_transient_message(window.video(), _("Hotkeys Reset"), _("All hotkeys have been reset to their default values."),
			std::string(), false, false, true);
	clear_hotkeys();
	setup_hotkey_list(window);
	window.invalidate_layout();
}

void tpreferences::remove_hotkey_callback(tlistbox& hotkeys)
{
	int row_number = hotkeys.get_selected_row();
	const hotkey::hotkey_command& hotkey_item = *visible_hotkeys_[row_number];
	hotkey::clear_hotkeys(hotkey_item.command);
	find_widget<tlabel>(hotkeys.get_row_grid(row_number), "lbl_hotkey", false).set_label(hotkey::get_names(hotkey_item.command));
}

void tpreferences::on_advanced_prefs_list_select(tlistbox& list, twindow& window)
{
	const int selected_row = list.get_selected_row();

	const ADVANCED_PREF_TYPE& selected_type = ADVANCED_PREF_TYPE::string_to_enum(
		adv_preferences_cfg_[selected_row]["type"].str());

	const std::string& selected_field = adv_preferences_cfg_[selected_row]["field"].str();

	if(selected_type == ADVANCED_PREF_TYPE::SPECIAL) {
		if(selected_field == "advanced_graphic_options") {
			gui2::tadvanced_graphics_options::display(window.video());
		} else if(selected_field == "logging") {
			gui2::tlogging::display(window.video());
		} else if(selected_field == "orb_color") {
			gui2::tselect_orb_colors::display(window.video());
		} else {
			WRN_GUI_L << "Invalid or unimplemented custom advanced prefs option: " << selected_field << "\n";
		}

		// Add more options here as needed
	}

	const bool has_description = !adv_preferences_cfg_[selected_row]["description"].empty();

	if(has_description || (selected_type != ADVANCED_PREF_TYPE::SPECIAL && selected_type != ADVANCED_PREF_TYPE::TOGGLE)) {
		find_widget<twidget>(get_advanced_row_grid(list, selected_row), "prefs_setter_grid", false)
			.set_visible(tcontrol::tvisible::visible);
	}

	if(last_selected_item_ != selected_row) {
		find_widget<twidget>(get_advanced_row_grid(list, last_selected_item_), "prefs_setter_grid", false)
			.set_visible(tcontrol::tvisible::invisible);

		last_selected_item_ = selected_row;
	}
}

void tpreferences::initialize_tabs(twindow& /*window*/, tlistbox& selector)
{
	//
	// MULTIPLAYER TABS
	//

#ifdef GUI2_EXPERIMENTAL_LISTBOX
	connect_signal_notify_modified(selector, std::bind(
			&tpreferences::on_tab_select,
			this,
			std::ref(window)));
#else
	selector.set_callback_value_change(dialog_callback
			<tpreferences, &tpreferences::on_tab_select>);
#endif
}

static int index_in_pager_range(const int& first, const tstacked_widget& pager)
{
	// Ensure the specified index is between 0 and one less than the max
	// number of pager layers (since get_layer_count returns one-past-end).
	return std::min<int>(std::max(0, first), pager.get_layer_count() - 1);
}

void tpreferences::pre_show(twindow& window)
{
	set_always_save_fields(true);

	//
	// Status labels
	// These need to be set here in pre_show, once the fields are initialized. For some reason, this
	// is not the case for those in Advanced
	//

	gui2::bind_status_label<tslider>(window, "max_saves_slider", [](tslider& s)->std::string {
		return s.get_value() == INFINITE_AUTO_SAVES ? _("∞") : s.get_value_label().str();
	});

	gui2::bind_status_label<tslider>(window, "turbo_slider",     [](tslider& s)->std::string {
		return s.get_value_label();
	});

	gui2::bind_status_label<tslider>(window, "scaling_slider",   [](tslider& s)->std::string {
		return s.get_value_label() + "%";
	});

	tlistbox& selector = find_widget<tlistbox>(&window, "selector", false);
	tstacked_widget& pager = find_widget<tstacked_widget>(&window, "pager", false);

#ifdef GUI2_EXPERIMENTAL_LISTBOX
	connect_signal_notify_modified(selector, std::bind(
			&tpreferences::on_page_select,
			this,
			std::ref(window)));
#else
	selector.set_callback_value_change(dialog_callback
			<tpreferences, &tpreferences::on_page_select>);
#endif
	window.keyboard_capture(&selector);

	VALIDATE(selector.get_item_count() == pager.get_layer_count(),
		"The preferences pager and its selector listbox do not have the same number of items.");

	const int main_index = index_in_pager_range(initial_index_.first, pager);

	// Loops through each pager layer and checks if it has both a tab bar
	// and stack. If so, it initilizes the options for the former and
	// selects the specified layer of the latter.
	for(unsigned int i = 0; i < pager.get_layer_count(); ++i) {
		tlistbox* tab_selector = find_widget<tlistbox>(
			pager.get_layer_grid(i), "tab_selector", false, false);

		tstacked_widget* tab_pager = find_widget<tstacked_widget>(
			pager.get_layer_grid(i), "tab_pager", false, false);

		if(tab_pager && tab_selector) {
			const int ii = static_cast<int>(i);
			const int tab_index = index_in_pager_range(initial_index_.second, *tab_pager);
			const int to_select = (ii == main_index ? tab_index : 0);

			// Initialize tabs for this page
			initialize_tabs(window, *tab_selector);

			tab_selector->select_row(to_select);
			tab_pager->select_layer(to_select);
		}
	}

	// Finally, select the initial main page
	selector.select_row(main_index);
	pager.select_layer(main_index);
}

void tpreferences::set_visible_page(twindow& window, unsigned int page, const std::string& pager_id)
{
	find_widget<tstacked_widget>(&window, pager_id, false).select_layer(page);
}

// Special fullsceen callback
void tpreferences::fullscreen_toggle_callback(twindow& window)
{
	const bool ison = find_widget<ttoggle_button>(&window, "fullscreen", false).get_value_bool();
	window.video().set_fullscreen(ison);

	tmenu_button& res_list = find_widget<tmenu_button>(&window, "resolution_set", false);

	set_resolution_list(res_list, window.video());
	res_list.set_active(!ison);
}

void tpreferences::handle_res_select(twindow& window)
{
	tmenu_button& res_list = find_widget<tmenu_button>(&window, "resolution_set", false);
	const int choice = res_list.get_value();

	if(resolutions_[static_cast<size_t>(choice)] == window.video().current_resolution()) {
		return;
	}

	window.video().set_resolution(resolutions_[static_cast<size_t>(choice)]);
	events::raise_resize_event();
	set_resolution_list(res_list, window.video());
}

void tpreferences::on_page_select(twindow& window)
{
	const int selected_row =
		std::max(0, find_widget<tlistbox>(&window, "selector", false).get_selected_row());
	set_visible_page(window, static_cast<unsigned int>(selected_row), "pager");
	// FIXME: This is a hack to ensure that the hotkey categories bar doesn't take up extra vertical space
	// It should be removed if the matrix placement policy can be fixed.
	if(selected_row == 1) {
		window.invalidate_layout();
	}
}

void tpreferences::on_tab_select(twindow& window)
{
	const int selected_row =
		std::max(0, find_widget<tlistbox>(&window, "tab_selector", false).get_selected_row());
	set_visible_page(window, static_cast<unsigned int>(selected_row), "tab_pager");
}

void tpreferences::post_show(twindow& /*window*/)
{
	save_hotkeys();
}

} // end namespace gui2
