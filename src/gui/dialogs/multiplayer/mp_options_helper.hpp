/*
   Copyright (C) 2008 - 2016 by the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_MP_OPTIONS_HELPER_HPP_INCLUDED
#define GUI_DIALOGS_MP_OPTIONS_HELPER_HPP_INCLUDED

#include "game_initialization/create_engine.hpp"
#include "gui/widgets/generator.hpp"

class config;

namespace gui2
{

class tcontrol;
class tmenu_button;
class ttoggle_button;
class ttree_view;
class ttree_view_node;
class twindow;

class tmp_options_helper
{
public:
	tmp_options_helper(twindow& window, ng::create_engine& create_engine);

	void update_all_options();

	void update_game_options();
	void update_era_options();
	void update_mod_options();

	config get_options_config();

private:
	struct option_source {
		std::string level_type;
		std::string id;
		friend bool operator<(const option_source& a, const option_source& b) {
			return a.level_type < b.level_type || (a.level_type == b.level_type && a.id < b.id);
		}
	};

	int remove_nodes_for_type(const std::string& type, int& saved_pos);

	using data_map = std::map<std::string, string_map>;

	template <typename T>
	std::pair<T*, config::attribute_value> add_node_and_get_widget(
		ttree_view_node& option_node, const std::string& id, data_map& data, const config& cfg);

	void display_custom_options(const std::string& type, int node_position, const config& data);

	template<typename T>
	void update_options_data_map(T* widget, const option_source& source);
	void update_options_data_map(ttoggle_button* widget, const option_source& source);

	// NOTE: this cannot be an overload of update_options_data_map since that's a templated function
	void update_options_data_map_menu_button(tmenu_button* widget, const option_source& source, const config& cfg);

	void reset_options_data(const option_source& source, bool& handled, bool& halt);

	void update_status_label();

	ng::create_engine& create_engine_;

	ttree_view& options_tree_;
	tcontrol& no_options_notice_;

	int last_game_pos_;
	int last_era_pos_;
	int last_mod_pos_;

	using node_vector = std::vector<ttree_view_node*>;
	std::map<std::string, node_vector> node_map_;

	std::vector<option_source> visible_options_;
	std::map<std::string, config> options_data_;

public:
	const std::vector<option_source>& get_visible_options() const
	{
		return visible_options_;
	}

	std::map<std::string, config>& get_options_data()
	{
		return options_data_;
	}
};

} // namespace gui2

#endif
