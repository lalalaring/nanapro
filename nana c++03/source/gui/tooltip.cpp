/*
 *	A Tooltip Implementation
 *	Copyright(C) 2003-2013 Jinhao(cnjinhao@hotmail.com)
 *
 *	Distributed under the Boost Software License, Version 1.0.
 *	(See accompanying file LICENSE_1_0.txt or copy at
 *	http://www.boost.org/LICENSE_1_0.txt)
 *
 *	@file: nana/gui/tooltip.cpp
 */

#include <nana/gui/tooltip.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/timer.hpp>
#include <nana/memory.hpp>

namespace nana{ namespace gui{
	namespace drawerbase
	{
		namespace tooltip
		{
			class drawer
				: public nana::gui::drawer_trigger
			{
			private:
				void refresh(graph_reference graph)
				{
					graph.rectangle(0, 0, graph.width(), graph.height(), 0x0, false);
					graph.rectangle(1, 1, graph.width() - 2, graph.height() - 2, 0xF0F0F0, true);
				}
			};	//end class drawer

			nana::point pos_by_screen(nana::point pos, const nana::size& sz)
			{
				const nana::rectangle scr_area = API::screen_area_from_point(pos);
				if (pos.x + sz.width > scr_area.x + scr_area.width)
					pos.x = static_cast<int>(scr_area.x + scr_area.width - sz.width);
				if (pos.x < scr_area.x)
					pos.x = scr_area.x;

				if (pos.y + sz.height >= scr_area.y + scr_area.height)
					pos.y = static_cast<int>(scr_area.y + scr_area.height - sz.height);
				else
					pos.y += 20;

				if (pos.y < scr_area.y)
					pos.y = scr_area.y;

				return pos;
			}

			class tip_form
				:	public widget_object<category::root_tag, drawer>,
					public tooltip_interface
			{
				typedef widget_object<category::root_tag, drawer> base_type;
			public:
				tip_form()
					:base_type(rectangle(), appear::bald<appear::floating>())
				{
					API::take_active(this->handle(), false, 0);

					label_.create(*this);
					label_.format(true).transparent(true);
				}
			private:
				//tooltip_interface implementation
				void tooltip_text(const nana::string& text) //override
				{
					label_.caption(text);
					nana::size text_s = label_.measure(API::screen_size().width * 2 / 3);
					this->size(text_s.width + 10, text_s.height + 10);
					label_.move(5, 5, text_s.width, text_s.height);

					timer_.interval(500);
					timer_.make_tick(nana::make_fun(*this, &tip_form::_m_tick));
				}

				virtual nana::size tooltip_size() const //override
				{
					return this->size();
				}

				virtual void tooltip_move(const nana::point& scr_pos, bool ignore_pos) //override
				{
					ignore_pos_ = ignore_pos;
					pos_ = scr_pos;
				}
			private:
				void _m_tick()
				{
					nana::point pos;
					if (ignore_pos_)
					{
						pos = API::cursor_position();
						
						//The cursor must be stay here for half second.
						if (pos != pos_)
						{
							pos_ = pos;
							return;
						}

						pos = pos_by_screen(pos, size());
					}
					else
						pos = pos_;

					timer_.enable(false);
					move(pos.x, pos.y);
					show();
				}

			private:
				timer timer_;
				gui::label	label_;
				nana::point	pos_;
				bool		ignore_pos_;
			};//end class tip_form


			class controller
			{
				typedef std::pair<window, nana::string> pair_t;

				class tip_form_factory
					: public gui::tooltip::factory_if_type
				{
					tooltip_interface * create() //override
					{
						return new tip_form;
					}

					void destroy(tooltip_interface* p) //override
					{
						delete p;
					}
				};

				class deleter
				{
				public:
					deleter(nana::shared_ptr<gui::tooltip::factory_if_type>& fp)
						:fp_(fp)
					{}

					void operator()(tooltip_interface* p)
					{
						fp_->destroy(p);
					}
				private:
					nana::shared_ptr<gui::tooltip::factory_if_type> fp_;
				};
			public:
				static nana::shared_ptr<gui::tooltip::factory_if_type>& factory()
				{
					static nana::shared_ptr<gui::tooltip::factory_if_type> fp;
					if (!fp)
						fp.reset(new tip_form_factory());

					return fp;
				}

				//external synchronization.
				static controller* instance(bool destroy = false)
				{
					static controller* ptr;
					if(destroy)
					{
						delete ptr;
						ptr = 0;
					}
					else if(ptr == 0)
					{
						ptr = new controller;
					}
					return ptr;
				}

				void set(window wd, const nana::string& str)
				{
					if (str.empty())
						_m_untip(wd);
					else
						_m_get(wd).second = str;
				}

				void show(const nana::string& text)
				{
					if (!window_)
						window_ = nana::shared_ptr<tooltip_interface>(factory()->create(), deleter(factory()));

					window_->tooltip_text(text);
					window_->tooltip_move(API::cursor_position(), true);
				}

				void show(point pos, const nana::string& text)
				{
					if (!window_)
						window_ = nana::shared_ptr<tooltip_interface>(factory()->create(), deleter(factory()));

					window_->tooltip_text(text);
					pos = pos_by_screen(pos, window_->tooltip_size());
					window_->tooltip_move(pos, false);
				}

				void close()
				{
					window_.reset();

					//Destroy the tooltip controller when there are not tooltips.
					if (cont_.empty())
						instance(true);
				}
			private:
				void _m_enter(const eventinfo& ei)
				{
					pair_t & pr = _m_get(ei.window);
					if(pr.second.size())
					{
						this->show(pr.second);
					}
				}

				void _m_leave(const eventinfo& ei)
				{
					close();
				}

				void _m_destroy(const eventinfo& ei)
				{
					_m_untip(ei.window);
				}


				void _m_untip(window wd)
				{
					for (std::vector<pair_t>::iterator i = cont_.begin(); i != cont_.end(); ++i)
					{
						if (i->first == wd)
						{
							cont_.erase(i);

							if (cont_.empty())
							{
								window_.reset();
								instance(true);
							}
							return;
						}
					}
				}
			private:
				pair_t& _m_get(nana::gui::window wd)
				{
					for(std::vector<pair_t>::iterator i = cont_.begin(); i != cont_.end(); ++i)
					{
						if(i->first == wd)
							return *i;
					}

					API::make_event<events::mouse_enter>(wd, nana::functor<void(const eventinfo&)>(*this, &controller::_m_enter));
					API::make_event<events::mouse_leave>(wd, nana::functor<void(const eventinfo&)>(*this, &controller::_m_leave));
					API::make_event<events::mouse_down>(wd, nana::functor<void(const eventinfo&)>(*this, &controller::_m_leave));
					API::make_event<events::destroy>(wd, nana::functor<void(const eventinfo&)>(*this, &controller::_m_destroy));

					cont_.push_back(pair_t(wd, nana::string()));
					return cont_.back();
				}
			private:
				nana::shared_ptr<tooltip_interface> window_;
				std::vector<pair_t> cont_;
			};
		}//namespace tooltip
	}//namespace drawerbase

	//class tooltip
		typedef drawerbase::tooltip::controller ctrl;

		void tooltip::set(nana::gui::window wd, const nana::string& text)
		{
			if(false == API::empty_window(wd))
			{
				internal_scope_guard lock;
				ctrl::instance()->set(wd, text);
			}
		}

		void tooltip::show(window wd, int x, int y, const nana::string& text)
		{
			internal_scope_guard lock;
			nana::point pos(x, y);
			API::calc_screen_point(wd, pos);
			ctrl::instance()->show(pos, text);
		}

		void tooltip::close()
		{
			internal_scope_guard lock;
			ctrl::instance()->close();
		}

		void tooltip::_m_hold_factory(factory_interface* p)
		{
			ctrl::factory().reset(p);
		}
	//end class tooltip

}//namespace gui
}//namespace nana

