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

#ifndef GUI_WIDGETS_SIZE_FIXATER_HPP_INCLUDED
#define GUI_WIDGETS_SIZE_FIXATER_HPP_INCLUDED

#include <gui/widgets/container.hpp>

#include <gui/auxiliary/formula.hpp>
#include <gui/core/widget_definition.hpp>
#include <gui/core/window_builder.hpp>
#include <gui/widgets/generator.hpp>

namespace gui2
{

	namespace implementation
	{
		struct tbuilder_size_fixater;
	}

	/* A fixed-size widget that wraps an arbitrary widget and forces it to the given size. */

	class tsize_fixater : public tcontainer_
	{
		friend struct implementation::tbuilder_size_fixater;

	public:
		tsize_fixater() :
			tcontainer_(1),
			generator_(
				tgenerator_::build(false, false, tgenerator_::independent, false))
		{}

		/** See @ref tcontrol::get_active. */
		bool get_active() const override
		{
			return true;
		}

		/** See @ref tcontrol::get_state. */
		unsigned get_state() const override
		{
			return 0;
		}

		/** See @ref twidget::layout_children. */
		void layout_children() override;

		void set_size(const tpoint& size)
		{
			size_ = size;
		}

	protected:
		tpoint calculate_best_size() const override
		{
			return size_;
		}

	private:
		tpoint size_;

		/**
		 * Contains a pointer to the generator.
		 *
		 * The pointer is not owned by this class, it's stored in the content_grid_
		 * of the tcontainer_ base class and freed when its grid is freed.
		 */
		tgenerator_* generator_;

		/**
		 * Finishes the building initialization of the widget.
		 *
		 * @param widget_builder      The builder to build the contents of the
		 *                            widget.
		 */
		void finalize(tbuilder_grid_const_ptr widget_builder);

		/** See @ref tcontrol::get_control_type. */
		const std::string& get_control_type() const override
		{
			static const std::string control_type = "size_fixater";
			return control_type;
		}

		/** See @ref tcontainer_::set_self_active. */
		void set_self_active(const bool) override
		{
			// DO NOTHING
		}
	};

	struct tsize_fixater_definition : public tcontrol_definition
	{
		explicit tsize_fixater_definition(const config& cfg);

		struct tresolution : public tresolution_definition_
		{
			explicit tresolution(const config& cfg);

			tbuilder_grid_ptr grid;
		};
	};

	namespace implementation
	{

		struct tbuilder_size_fixater : public tbuilder_control
		{
			explicit tbuilder_size_fixater(const config& cfg);

			using tbuilder_control::build;

			twidget* build() const;

		private:
			tbuilder_grid_const_ptr content_;
			tformula<unsigned> width_;
			tformula<unsigned> height_;
		};
	}
}

#endif