#pragma once
#include "basic_demo_app.hh"

#include "scene/main_scene.hh"
#include "scene/engine_scene.hh"
#include "scene/pcd_scene.hh"
#include "scene/bvh_scene.hh"

namespace demo
{
    class demo_scene_control_window
        : public triengine::gui::iwindow
    {
    private:
        triengine::visualization::visualizer* const _vis;

        std::list<std::shared_ptr<scene_wrapper>> _scn_list;
        std::list<std::shared_ptr<scene_wrapper>>::iterator _curr_scn_it{ _scn_list.end() };
        std::unordered_map<
            triengine::scene_id_t,
            std::list<std::shared_ptr<scene_wrapper>>::iterator
        > _scn_id_map;
        
    public:
        demo_scene_control_window(triengine::visualization::visualizer* vis)
            : _vis{ vis }
        {}

        virtual ~demo_scene_control_window() = default;

        const char* get_window_name() const override {
            return "Scene Controller";
        }

        ImVec2 get_initial_window_size() const override {
            return ImVec2{ 430.0f, 470.0f };
        }

        void render(
            [[maybe_unused]] const triengine::gui::window_render_context& render_ctx) override
        {
            if (_curr_scn_it == _scn_list.end()) {
                return;
            }

            if (_vis->get_current_scene()->get_id() != (*_curr_scn_it)->get_scene_id()) {
                return;
            }

            ImGui::Text("Current Scene: %s", (*_curr_scn_it)->get_scene()->get_name().c_str());
            if (ImGui::Button("<")) { this->switch_to_prev_scene(); }
            ImGui::SameLine();
            if (ImGui::Button(">")) { this->switch_to_next_scene(); }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            (*_curr_scn_it)->render_gui(render_ctx);
        }

        std::shared_ptr<const scene_wrapper> get_current_scene() const
        {
            return (_curr_scn_it != _scn_list.end())
                ? *_curr_scn_it
                : nullptr;
        }
        
        std::shared_ptr<scene_wrapper> get_current_scene()
        {
            return (_curr_scn_it != _scn_list.end())
                ? *_curr_scn_it
                : nullptr;
        }

        void add_scene(const std::shared_ptr<scene_wrapper>& new_scn)
        {
            const bool is_first{ _scn_list.empty() };

            _scn_list.push_back(new_scn);
            _scn_id_map[new_scn->get_scene_id()] = std::prev(_scn_list.end());

            if (is_first) {
                _curr_scn_it = std::prev(_scn_list.end());
                _vis->switch_scene((*_curr_scn_it)->get_scene_id());
            }
        }

        void switch_to_prev_scene()
        {
            if (_curr_scn_it != _scn_list.end()) {
                _curr_scn_it = std::prev((_curr_scn_it != _scn_list.begin())
                    ? _curr_scn_it
                    : _scn_list.end()
                );
                _vis->switch_scene((*_curr_scn_it)->get_scene_id());
            }
        }

        void switch_to_next_scene()
        {
            if (_curr_scn_it != _scn_list.end()) {
                const auto next_it = std::next(_curr_scn_it);
                _curr_scn_it = (next_it != _scn_list.end())
                    ? next_it
                    : _scn_list.begin();
                _vis->switch_scene((*_curr_scn_it)->get_scene_id());
            }
        }

        void update_animation()
        {
            if (_curr_scn_it != _scn_list.end()) {
                (*_curr_scn_it)->update_animation();
            }
        }

    }; // class

    basic_demo_app::basic_demo_app()
    {}

    basic_demo_app::~basic_demo_app()
    {}

    void basic_demo_app::create()
    {
        CXLIB_TRACE("{}() ENTER", __func__);

        _vis = std::make_unique<triengine::visualization::visualizer>();
        _vis->create_window("Triengine Demo"
            " (Build: " __DATE__ ", " __TIME__
#if defined (_DEBUG)
            " DBG"
#else  // ^^^ _DEBUG ^^^ / vvv !_DEBUG vvv
            " REL"
#endif // ^^^ !_DEBUG ^^^
            ")"
        );

        _vis->set_key_callback(
            [this](
                [[maybe_unused]] const int32_t key,
                [[maybe_unused]] const int32_t scancode,
                [[maybe_unused]] const int32_t action,
                [[maybe_unused]] const int32_t mods,
                [[maybe_unused]] bool& handled)
            {
                if (action != GLFW_RELEASE)
                {
                    CXLIB_TRACE("key: {}", key);

                    switch (key) {
                    case GLFW_KEY_ESCAPE:
                        break;
                    case GLFW_KEY_F12:
                        _vis->enable_main_menu(!_vis->is_main_menu_enabled());
                        break;
                    case GLFW_KEY_SPACE:
                        _flag_animation = !_flag_animation;
                        break;
                    case GLFW_KEY_LEFT:
                        _demo_scene_ctrl_window->switch_to_prev_scene();
                        break;
                    case GLFW_KEY_RIGHT:
                        _demo_scene_ctrl_window->switch_to_next_scene();
                        break;
                    }
                }
            });

        _log_window = std::make_shared<triengine::gui::log_window>();
        _log_window->set_visible(false);
        _vis->add_gui_window(_log_window);

        _render_stats_window = std::make_shared<triengine::gui::render_stats_window>();
        _render_stats_window->set_visible(false);
        _vis->add_gui_window(_render_stats_window);

        _demo_scene_ctrl_window = std::make_shared<demo_scene_control_window>(_vis.get());
        _demo_scene_ctrl_window->set_visible(true);
        _vis->add_gui_window(_demo_scene_ctrl_window);

        _demo_scene_ctrl_window->add_scene(std::make_shared<scene::main_scene>(*_vis));
        _demo_scene_ctrl_window->add_scene(std::make_shared<scene::engine_scene>(*_vis));
        _demo_scene_ctrl_window->add_scene(std::make_shared<scene::pointcloud_scene>(*_vis));
        _demo_scene_ctrl_window->add_scene(std::make_shared<scene::bvh_scene>(*_vis));
    }

    void basic_demo_app::destroy()
    {
        CXLIB_TRACE("{}() ENTER", __func__);

        _log_window.reset();
        _render_stats_window.reset();
        
        _vis->destroy_window();
        _vis.reset();

        CXLIB_TRACE("{}() LEAVE", __func__);
    }

    void basic_demo_app::run()
    {
        CXLIB_TRACE("{}() ENTER", __func__);

        CXLIB_DEBUG("polling start..");
        while (_vis->update_window())
        {
            // animate
            if (_flag_animation) {
                _demo_scene_ctrl_window->update_animation();
            }

            _vis->render();
        } // while

        CXLIB_TRACE("{}() LEAVE", __func__);
    }

} // namespace