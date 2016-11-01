/*
   Copyright (C) 2016 Jyrki Vesterinen <sandgtx@gmail.com>
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

#include "size_lock.hpp"

#include <gettext.hpp>
#include <gui/core/register_widget.hpp>
#include <gui/widgets/helper.hpp>
#include <gui/widgets/settings.hpp>
#include <wml_exception.hpp>

namespace gui2
{

REGISTER_WIDGET(size_lock)

void tsize_lock::layout_children()
{
	assert(generator_ != nullptr);
	assert(generator_->get_item_count() == 1);

	generator_->item(0).layout_children();
}

void tsize_lock::finalize(tbuilder_grid_const_ptr widget_builder)
{
	assert(generator_ != nullptr);
	
	generator_->create_item(-1, widget_builder, string_map(), nullptr);

	static const std::string id = "_content_grid";
	grid().set_id(id);
	twidget* old_grid = grid().swap_child(id, generator_, true);
	delete old_grid;

	generator_->select_item(0u, true);
}

tsize_lock_definition::tsize_lock_definition(const config& cfg) :
	tcontrol_definition(cfg)
{
	DBG_GUI_P << "Parsing fixed size widget " << id << '\n';

	load_resolutions<tresolution>(cfg);
}

/*WIKI
 * @page = GUIWidgetDefinitionWML
 * @order = 1_size_lock
 *
 * == Size lock ==
 *
 * A size lock contains one child widget and forces it to have the specified size.
 * This can be used, for example, when there are two list boxes in different rows of
 * the same grid and it's desired that only one list box changes size when its
 * contents change.
 *
 * A size lock has no states.
 * @begin{parent}{name="gui/"}
 * @begin{tag}{name="size_lock_definition"}{min=0}{max=-1}{super="generic/widget_definition"}
 * @end{tag}{name="size_lock_definition"}
 * @end{tag}{name="gui/"}
 */
tsize_lock_definition::tresolution::tresolution(const config& cfg) :
	tresolution_definition_(cfg), grid(nullptr)
{
	// Add a dummy state since every widget needs a state.
	static config dummy("draw");
	state.push_back(tstate_definition(dummy));

	const config& child = cfg.child("grid");
	VALIDATE(child, _("No grid defined."));

	grid = std::make_shared<tbuilder_grid>(child);
}

/*WIKI
 * @page = GUIWidgetInstanceWML
 * @order = 2_size_lock
 * @begin{parent}{name="gui/window/resolution/grid/row/column/"}
 * @begin{tag}{name="size_lock"}{min=0}{max=-1}{super="generic/widget_instance"}
 * == Size lock ==
 *
 * A size lock contains one child widget and forces it to have the specified size.
 * This can be used, for example, when there are two list boxes in different rows of
 * the same grid and it's desired that only one list box changes size when its
 * contents change.
 *
 * @begin{table}{config}
 *    widget & section    & mandatory &           The widget. $
 *    width  & f_unsigned & mandatory &           The width of the widget. $
 *    height & f_unsigned & mandatory &           The height of the widget. $
 * @end{table}
 *
 * The variables available are the same as for window resolution, see
 * [[GuiToolkitWML#Resolution_2]] for the list of items.
 * @end{tag}{name="size_lock"}
 * @end{parent}{name="gui/window/resolution/grid/row/column/"}
 */

namespace implementation
{

tbuilder_size_lock::tbuilder_size_lock(const config& cfg) :
	tbuilder_control(cfg), content_(nullptr), width_(cfg["width"]), height_(cfg["height"])
{
	VALIDATE(cfg.has_child("widget"), _("No widget defined."));
	content_ = std::make_shared<tbuilder_grid>(cfg.child("widget"));
}

twidget* tbuilder_size_lock::build() const
{
	tsize_lock* widget = new tsize_lock();

	init_control(widget);

	DBG_GUI_G << "Window builder: placed fixed size widget '" << id <<
		"' with definition '" << definition << "'.\n";

	auto conf = std::static_pointer_cast<const tsize_lock_definition::tresolution>(widget->config());
	assert(conf != nullptr);

	widget->init_grid(conf->grid);

	game_logic::map_formula_callable size = get_screen_size_variables();

	const unsigned width = width_(size);
	const unsigned height = height_(size);

	VALIDATE(width > 0 && height > 0, _("Invalid size."));

	widget->set_size(tpoint(width, height));

	widget->finalize(content_);

	return widget;
}

}

}
