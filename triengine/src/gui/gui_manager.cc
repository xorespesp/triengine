#include "gui_manager.hh"
#include "../misc/debug_utils.hh"
#include "../visualizer_window.hh"
#include "../shader/shader_version.h"
#include "../extern/fonts/Fonts.h"

#include <GLFW/glfw3.h>

namespace triengine::gui
{
    /**
     * [basic references]
     * https://dlemrcnd.tistory.com/65
     *
     * [about design pattern]
     * https://github.com/ocornut/imgui/issues/5032
     * https://github.com/RuurddeRonde/ImGuiUI
     * https://github.com/TheCherno/Hazel/tree/master/Hazel/src/Hazel
     *
     */

    namespace {
        namespace detail
        {
            static constexpr float kDefaultFontSize = 15.0f;
            static constexpr const char kMainDockSpaceName[] = "MyMainDockSpace";

            inline float GetTitleBarHeight() {
                return ImGui::GetFont()->FontSize + ImGui::GetStyle().FramePadding.y * 2;
            }
        } // namespace
    }

    gui_manager::gui_manager() {}
    gui_manager::~gui_manager() {
        if (this->is_initialized()) {
            this->deinitialize();
        }
    }

    bool gui_manager::is_initialized() const noexcept {
        return _flag_initialized;
    }

    void gui_manager::initialize(
        visualizer_window* vis_window,
        const float dpi_scale_factor)
    {
        TRIENGINE_ASSERT(vis_window != nullptr);

        _vis_window = vis_window;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // Enable Docking (Ref: https://github.com/ocornut/imgui/wiki/Docking)

        // Let's not attempt to do a fopen() of the imgui.ini file.
        // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
        io.IniFilename = nullptr;
        io.ConfigWindowsMoveFromTitleBarOnly = true; // Ref: https://github.com/ocornut/imgui/issues/899#issuecomment-446170903

        //io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesKorean());

        // Setup Platform/Renderer backends
        if (!::ImGui_ImplGlfw_InitForOpenGL(vis_window->get_glfw_window(), true)) {
            TRIENGINE_PANIC("ImGui_ImplGlfw_InitForOpenGL failed");
        }

        if (!::ImGui_ImplOpenGL3_Init(shader::glslShaderVersion)) {
            TRIENGINE_PANIC("ImGui_ImplOpenGL3_Init failed");
        }

        this->setup_imgui_style();

        _flag_initialized = true;
        this->change_dpi_scale(dpi_scale_factor);

        _scene_window = std::make_shared<gui::scene_view_window>(_vis_window);
        this->add_window(_scene_window);

        _scene_ctrl_window = std::make_shared<gui::scene_control_window>(_vis_window);
        this->add_window(_scene_ctrl_window);
    }

    void gui_manager::deinitialize()
    {
        if (_flag_initialized)
        {
            _flag_initialized = false;

            ::ImGui_ImplOpenGL3_Shutdown();
            ::ImGui_ImplGlfw_Shutdown();

            ImPlot::DestroyContext();
            ImGui::DestroyContext();
        }
    }

    void gui_manager::change_dpi_scale(
        const float scale_factor)
    {
        TRIENGINE_ASSERT(this->is_initialized());

        _dpi_scale_factor = scale_factor;

        this->setup_imgui_fonts(scale_factor);

        // FIXME: handle dpi change correctly
        // https://github.com/ocornut/imgui/issues/3757
        // https://github.com/ocornut/imgui/issues/1676
        ImGui::GetStyle().ScaleAllSizes(scale_factor);

        // ImGui doesn't automatically scale fonts, so we have to do that ourselves
        //ImFontConfig fontConfig{};
        //fontConfig.SizePixels = kDefaultFontSize * scale_factor;
        //ImGui::GetIO().Fonts->AddFontDefault(&fontConfig);
    }

    void gui_manager::render()
    {
        // Start the Dear ImGui frame
        ::ImGui_ImplOpenGL3_NewFrame();
        ::ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImGuiID main_dockspace_id = ImGui::GetID(detail::kMainDockSpaceName);
        ImGuiViewport* const viewport = ImGui::GetMainViewport();
        ImGui::DockSpaceOverViewport(main_dockspace_id, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

        const float scaled_padding_size = 30.0f * _dpi_scale_factor;

        // Show main menu bar
        // https://github.com/ocornut/imgui/issues/6307
        float main_menu_height{};
        if (_flag_show_main_menu && ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open", "Ctrl+O")) {
                    // ...
                }

                if (ImGui::MenuItem("Save", "Ctrl+S")) {
                    // ...
                }

                ImGui::MenuItem("Main menu", nullptr, &_flag_show_main_menu);

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Windows"))
            {
                for (auto window : _windows) {
                    bool p_selected = window->is_visible();
                    if (ImGui::MenuItem(window->get_window_name(), nullptr, &p_selected)) {
                        window->set_visible(p_selected);
                    }
                }

                if (ImGui::BeginMenu("Demo")) {
                    ImGui::MenuItem("ImGui Demo", nullptr, &_flag_show_imgui_demo_window);
                    ImGui::MenuItem("ImPlot Demo", nullptr, &_flag_show_implot_demo_window);
                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            main_menu_height = ImGui::GetWindowHeight();
            ImGui::EndMainMenuBar();
        }

        // Show windows
        {
            ImVec2 next_window_pos{ scaled_padding_size, main_menu_height + scaled_padding_size };
            ImGui::SetNextWindowPos(next_window_pos, ImGuiCond_Once);
            for (auto& window : _windows) {
                if (this->render_window(window)) {
                    next_window_pos.x += scaled_padding_size;
                    next_window_pos.y += scaled_padding_size;
                    ImGui::SetNextWindowPos(next_window_pos, ImGuiCond_Once);
                }
            }
        }

        if (_flag_show_imgui_demo_window) {
            ImGui::ShowDemoWindow(&_flag_show_imgui_demo_window);
        }

        if (_flag_show_implot_demo_window) {
            ImPlot::ShowDemoWindow(&_flag_show_implot_demo_window);
        }

        {
            thread_local bool is_first_loop = true;
            if (is_first_loop) {
                is_first_loop = false;
                this->setup_dock_space(main_dockspace_id, viewport);
            }
        }

        // Rendering
        ImGui::Render();
        ::ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    bool gui_manager::render_window(
        std::shared_ptr<iwindow> window)
    {
        bool is_opened = window->is_visible();
        if (is_opened)
        {
            ImVec2 next_window_size = window->get_initial_window_size();
            next_window_size.x *= _dpi_scale_factor;
            next_window_size.y *= _dpi_scale_factor;
            ImGui::SetNextWindowSize(next_window_size, ImGuiCond_Once);

            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse;
            window->pre_render(window_flags);

            if (ImGui::Begin(window->get_window_name(), &is_opened, window_flags)) {
                window_render_context render_ctx;
                render_ctx.dpi_scale = _dpi_scale_factor;
                window->render(render_ctx);
            }

            // Always call a matching End() for each Begin() call, regardless of its return value!
            ImGui::End();
            window->post_render();
        }

        window->set_visible(is_opened);
        return is_opened;
    }

    void gui_manager::setup_imgui_style()
    {
        // based on janekb04's Deep dark style
        // https://github.com/ocornut/imgui/issues/707

        ImGui::StyleColorsClassic();
        ImVec4* colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.9f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
        colors[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.69f);// ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.40f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
        colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
        colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
        colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
        colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
        colors[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
        colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
        colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.35f);
#ifdef IMGUI_HAS_DOCK
        colors[ImGuiCol_DockingPreview] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
        colors[ImGuiCol_DockingEmptyBg] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
#endif

        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowPadding = ImVec2(8.00f, 8.00f);
        style.FramePadding = ImVec2(5.00f, 2.00f);
        style.CellPadding = ImVec2(6.00f, 6.00f);
        style.ItemSpacing = ImVec2(6.00f, 6.00f);
        style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
        style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
        style.IndentSpacing = 25.0f;
        style.ScrollbarSize = 15.0f;
        style.GrabMinSize = 10.0f;
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.TabBorderSize = 1.0f;
        style.WindowRounding = 5.0f;
        style.ChildRounding = 4.0f;
        style.FrameRounding = 3.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 9.0f;
        style.GrabRounding = 3.0f;
        style.LogSliderDeadzone = 4.0f;
        style.TabRounding = 4.0f;

        // Setup ImPlot styles

        ImPlot::StyleColorsAuto();

        ImVec4* pcolors = ImPlot::GetStyle().Colors;
        pcolors[ImPlotCol_PlotBg] = ImVec4(0, 0, 0, 0);
        pcolors[ImPlotCol_PlotBorder] = ImVec4(0, 0, 0, 0);
        pcolors[ImPlotCol_Selection] = ImVec4(0.821f, 1.000f, 0.000f, 1.000f);;
        pcolors[ImPlotCol_Crosshairs] = colors[ImGuiCol_Text];

        ImPlot::GetStyle().DigitalBitHeight = 20;

        auto& pstyle = ImPlot::GetStyle();
        pstyle.PlotPadding = { 12, 12 };
        pstyle.LabelPadding = { 6, 6 };
        pstyle.LegendSpacing = { 10, 2 };
        pstyle.AnnotationPadding = { 4,2 };
        pstyle.LegendPadding = { 4, 4 };
        pstyle.LegendInnerPadding = { 4, 4 };
        pstyle.LegendSpacing = { 5, 1 };

        constexpr std::array<ImU32/* rgba */, 10> dracula_colors{
            4288967266, 4285315327, 4286315088, 4283782655, 4294546365,
            4287429361, 4291197439, 4294830475, 4294113528, 4284106564
        };

        constexpr std::array<ImU32/* rgba */, 20> custom_colors{
            4293363186, 4292735621, 4286954994, 4294083997, 4286968460,
            4289496562, 4294102149, 4286968556, 4294084056, 4290310789,
            4286945778, 4294087045, 4286968497, 4291855858, 4294111365,
            4286960626, 4294084020, 4287951493, 4287989234, 4294096261
        };

        [[maybe_unused]] const ImPlotColormap dracula_colormap = ImPlot::AddColormap("Dracula", dracula_colors.data(), static_cast<int>(dracula_colors.size()));
        [[maybe_unused]] const ImPlotColormap custom_colormap = ImPlot::AddColormap("Custom", custom_colors.data(), static_cast<int>(custom_colors.size()));

        ImPlot::GetStyle().Colormap = custom_colormap;
    }

    void gui_manager::setup_imgui_fonts(const float scale_factor)
    {
        ImGuiIO& io = ImGui::GetIO();

        // Reset fonts
        io.Fonts->Clear();
        _fonts_map.clear();

        // ImGui doesn't automatically scale fonts, so we have to do that ourselves
        const float font_size_pixels = detail::kDefaultFontSize * scale_factor;

        ImFontConfig font_cfg;
        font_cfg.FontDataOwnedByAtlas = false;

        ImFontConfig icons_config;
        icons_config.MergeMode = true;
        icons_config.PixelSnapH = true;
        icons_config.GlyphMinAdvanceX = 14.0f;
        icons_config.GlyphOffset = ImVec2(0, 0);
        icons_config.OversampleH = 1;
        icons_config.OversampleV = 1;
        icons_config.FontDataOwnedByAtlas = false;

        static const ImWchar fa_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

        ImStrncpy(font_cfg.Name, "Roboto Bold", 40);
        _fonts_map[font_cfg.Name] = io.Fonts->AddFontFromMemoryTTF(Roboto_Bold_ttf, Roboto_Bold_ttf_len, font_size_pixels, &font_cfg);
        io.Fonts->AddFontFromMemoryTTF(fa_solid_900_ttf, fa_solid_900_ttf_len, 14.0f, &icons_config, fa_ranges);

        ImStrncpy(font_cfg.Name, "Roboto Italic", 40);
        _fonts_map[font_cfg.Name] = io.Fonts->AddFontFromMemoryTTF(Roboto_Italic_ttf, Roboto_Italic_ttf_len, font_size_pixels, &font_cfg);
        io.Fonts->AddFontFromMemoryTTF(fa_solid_900_ttf, fa_solid_900_ttf_len, 14.0f, &icons_config, fa_ranges);

        ImStrncpy(font_cfg.Name, "Roboto Regular", 40);
        _fonts_map[font_cfg.Name] = io.Fonts->AddFontFromMemoryTTF(Roboto_Regular_ttf, Roboto_Regular_ttf_len, font_size_pixels, &font_cfg);
        io.Fonts->AddFontFromMemoryTTF(fa_solid_900_ttf, fa_solid_900_ttf_len, 14.0f, &icons_config, fa_ranges);

        ImStrncpy(font_cfg.Name, "Roboto Mono Bold", 40);
        _fonts_map[font_cfg.Name] = io.Fonts->AddFontFromMemoryTTF(RobotoMono_Bold_ttf, RobotoMono_Bold_ttf_len, font_size_pixels, &font_cfg);
        io.Fonts->AddFontFromMemoryTTF(fa_solid_900_ttf, fa_solid_900_ttf_len, 14.0f, &icons_config, fa_ranges);

        ImStrncpy(font_cfg.Name, "Roboto Mono Italic", 40);
        _fonts_map[font_cfg.Name] = io.Fonts->AddFontFromMemoryTTF(RobotoMono_Italic_ttf, RobotoMono_Italic_ttf_len, font_size_pixels, &font_cfg);
        io.Fonts->AddFontFromMemoryTTF(fa_solid_900_ttf, fa_solid_900_ttf_len, 14.0f, &icons_config, fa_ranges);

        ImStrncpy(font_cfg.Name, "Roboto Mono Regular", 40);
        _fonts_map[font_cfg.Name] = io.Fonts->AddFontFromMemoryTTF(RobotoMono_Regular_ttf, RobotoMono_Regular_ttf_len, font_size_pixels, &font_cfg);
        io.Fonts->AddFontFromMemoryTTF(fa_solid_900_ttf, fa_solid_900_ttf_len, 14.0f, &icons_config, fa_ranges);

        io.FontDefault = _fonts_map["Roboto Mono Bold"];
    }

    void gui_manager::setup_dock_space(
        const ImGuiID main_dockspace_id,
        ImGuiViewport* const viewport)
    {
        // Refs:
        // https://github.com/ocornut/imgui/issues/4430
        // https://gist.github.com/moebiussurfing/d7e6ec46a44985dd557d7678ddfeda99
        // https://github.com/moebiussurfing/ofxSurfingImGui/blob/master/3_Docking/3_0_Layout_Docking2/src/ofApp.cpp#L324-L361

        ImGui::DockBuilderRemoveNode(main_dockspace_id); // clear any previous layout
        ImGui::DockBuilderAddNode(main_dockspace_id, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(main_dockspace_id, viewport->Size);

        // -- split the dockspace into 2 nodes --
        // `ImGui::DockBuilderSplitNode()` takes in the following args in the following order:
        // node id to split, split direction, split size ratio (0.0 ~ 1.0 range),
        // the last two args(`out_id_at_dir`, `out_id_at_opposite_dir`) let's us choose which id we want 
        // (which ever one we DON'T set as `nullptr`, will be returned by the function)
        // `out_id_at_dir` is the id of the node in the direction we specified earlier,
        // `out_id_at_opposite_dir` is in the opposite direction

        ImGuiID dock_id_left, dock_id_right;
        dock_id_right = ImGui::DockBuilderSplitNode(
            main_dockspace_id,
            ImGuiDir_Right,
            0.75f,
            nullptr,
            &dock_id_left
        );

        ImGui::DockBuilderDockWindow(_scene_window->get_window_name(), dock_id_right);
        ImGui::DockBuilderDockWindow(_scene_ctrl_window->get_window_name(), dock_id_left);

        ImGui::DockBuilderFinish(main_dockspace_id);
    }

} // namespace