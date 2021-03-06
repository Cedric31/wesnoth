/*
   Copyright (C) 2008 - 2016 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_DIALOGS_TITLE_SCREEN_HPP_INCLUDED
#define GUI_DIALOGS_TITLE_SCREEN_HPP_INCLUDED

#include "gui/dialogs/dialog.hpp"

class game_launcher;

namespace gui2
{

class tpopup;

/** Do we wish to show the button for the debug clock. */
extern bool show_debug_clock_button;

/**
 * This class implements the title screen.
 *
 * The menu buttons return a result back to the caller with the button pressed.
 * So at the moment it only handles the tips itself.
 */
class ttitle_screen : public tdialog
{
public:
	ttitle_screen(game_launcher& game);

	~ttitle_screen();

	/**
	 * Values for actions which leave the title screen.
	 * Actions that merely show a dialog are not included here.
	 */
	enum tresult {
		// Start playing a single-player game, such as the tutorial or a campaign
		LAUNCH_GAME,
		// Connect to an MP server
		MP_CONNECT,
		// Host an MP game
		MP_HOST,
		// Start a local MP game
		MP_LOCAL,
		// Start the map/scenario editor
		MAP_EDITOR,
		// Show credits
		SHOW_ABOUT,
		// Exit to desktop
		QUIT_GAME,
		// Used to reload all game data
		RELOAD_GAME_DATA,
	};

	bool redraw_background() const
	{
		return redraw_background_;
	}

private:
	game_launcher& game_;

	bool redraw_background_;

	/** Inherited from tdialog, implemented by REGISTER_DIALOG. */
	virtual const std::string& window_id() const;

	/** Inherited from tdialog. */
	void pre_show(twindow& window);

	void on_resize(twindow& window);

	/** Holds the debug clock dialog. */
	tpopup* debug_clock_;

	/**
	 * Updates the tip of day widget.
	 *
	 * @param window              The window being shown.
	 * @param previous            Show the previous tip, else shows the next one.
	 */
	void update_tip(twindow& window, const bool previous);

	/** Shows the debug clock. */
	void show_debug_clock_window(CVideo& video);
};

} // namespace gui2

#endif
