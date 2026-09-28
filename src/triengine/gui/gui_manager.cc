#include "gui_manager.hh"
#include <triengine/utility/debug_utils.hh>
#include <triengine/visualization/visualizer.hh>
#include <triengine/core/shader_loader.hh>
#include <triengine/extern/fonts/Fonts.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

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

    ////////////////////////////////////////////////////////////////////////////////
    // gui_manager::about_dialog
    ////////////////////////////////////////////////////////////////////////////////

    class gui_manager::about_dialog
    {
    public:
        // True while the dialog or a demo window it opened is shown, or the dialog is about to open.
        bool is_visible() const noexcept {
            return _open_requested || _show_dialog || _show_imgui_demo || _show_implot_demo;
        }

        void request_open() noexcept {
            _open_requested = true;
        }

        void render(float dpi_scale)
        {
            if (_show_imgui_demo) { ImGui::ShowDemoWindow(&_show_imgui_demo); }
            if (_show_implot_demo) { ImPlot::ShowDemoWindow(&_show_implot_demo); }
            this->_render_dialog(dpi_scale);
        }

    private:
        using build_info_row = std::pair<const char*, std::string>;

        static void _draw_sierpinski(ImDrawList* draw_list, ImVec2 a, ImVec2 b, ImVec2 c, int depth, ImU32 color);
        static std::vector<build_info_row> _collect_build_info();

        void _render_dialog(float dpi_scale);

    private:
        bool _open_requested{ false };
        bool _show_dialog{ false };
        bool _show_imgui_demo{ false };
        bool _show_implot_demo{ false };
    };

    void gui_manager::about_dialog::_draw_sierpinski(ImDrawList* draw_list, ImVec2 a, ImVec2 b, ImVec2 c, int depth, ImU32 color)
    {
        if (depth == 0) {
            draw_list->AddTriangleFilled(a, b, c, color);
            return;
        }
        const auto mid = [](ImVec2 p, ImVec2 q) { return ImVec2{ (p.x + q.x) * 0.5f, (p.y + q.y) * 0.5f }; };
        const ImVec2 ab = mid(a, b), bc = mid(b, c), ca = mid(c, a);
        _draw_sierpinski(draw_list, a, ab, ca, depth - 1, color);
        _draw_sierpinski(draw_list, ab, b, bc, depth - 1, color);
        _draw_sierpinski(draw_list, ca, bc, c, depth - 1, color);
    }

    std::vector<gui_manager::about_dialog::build_info_row> gui_manager::about_dialog::_collect_build_info()
    {
        const auto gl_string = [](GLenum name) -> std::string {
            const auto* value = reinterpret_cast<const char*>(::glGetString(name));
            return value ? value : "unavailable";
        };

#if defined(_MSVC_LANG)
        constexpr long kCppStandard = _MSVC_LANG;
#else
        constexpr long kCppStandard = __cplusplus;
#endif
#if defined(_MSC_VER)
        const std::string compiler = "MSVC " + std::to_string(_MSC_VER / 100) + "." + std::to_string(_MSC_VER % 100);
#elif defined(__clang__)
        const std::string compiler = "Clang " __clang_version__;
#elif defined(__GNUC__)
        const std::string compiler = "GCC " __VERSION__;
#else
        const std::string compiler = "Unknown";
#endif
#if defined(_DEBUG)
        std::string configuration = "Debug";
#else
        std::string configuration = "Release";
#endif
#if defined(TRIENGINE_DEBUG_MODE)
        configuration += " (debug mode)";
#endif

        return {
            { "Build Date", __DATE__ " " __TIME__ },
            { "Configuration", configuration },
            { "Compiler", compiler + ", C++" + std::to_string((kCppStandard / 100) % 100) },
            { "Graphics API", "OpenGL " + gl_string(GL_VERSION) },
            { "Device", gl_string(GL_RENDERER) },
            { "Vendor", gl_string(GL_VENDOR) },
        };
    }

    void gui_manager::about_dialog::_render_dialog(float dpi_scale)
    {
        constexpr const char kPopupName[] = "About Triengine";
        // Opened here rather than from the menu item, whose ID stack differs from `BeginPopupModal()`'s.
        if (_open_requested) {
            ImGui::OpenPopup(kPopupName);
            _open_requested = false;
            _show_dialog = true;
        }

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, { 0.5f, 0.5f });
        if (!ImGui::BeginPopupModal(kPopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            _show_dialog = false;
            return;
        }

        const float logo_width = 80.0f * dpi_scale;
        const float logo_height = logo_width * 0.8660254f; // equilateral
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy({ logo_width, logo_height });
        // Clockwise, as ImGui's anti-aliased fill expects.
        _draw_sierpinski(ImGui::GetWindowDrawList(),
            { origin.x + logo_width * 0.5f, origin.y },
            { origin.x + logo_width, origin.y + logo_height },
            { origin.x, origin.y + logo_height },
            3, ImGui::GetColorU32(ImGuiCol_Text));

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 2.0f);
        ImGui::BeginGroup();
        ImGui::SetWindowFontScale(1.6f);
        ImGui::TextUnformatted("Triengine");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextDisabled("A lightweight 3D rendering engine");
        ImGui::EndGroup();

        ImGui::Separator();
        const auto build_info = _collect_build_info();
        if (ImGui::BeginTable("BuildInfo", 2, ImGuiTableFlags_SizingFixedFit)) {
            for (const auto& [label, value] : build_info) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextDisabled("%s", label);
                ImGui::TableNextColumn(); ImGui::TextUnformatted(value.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::Separator();

        if (ImGui::Button("Copy")) {
            std::string text = "Triengine\n";
            for (const auto& [label, value] : build_info) {
                text.append(label).append(": ").append(value).append("\n");
            }
            ImGui::SetClipboardText(text.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }
#if defined(TRIENGINE_DEBUG_MODE)
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 3.0f);
        if (ImGui::TextLink("ImGui Demo")) {
            _show_imgui_demo = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::TextLink("ImPlot Demo")) {
            _show_implot_demo = true;
            ImGui::CloseCurrentPopup();
        }
#endif
        ImGui::EndPopup();
    }

    ////////////////////////////////////////////////////////////////////////////////
    // gui_manager
    ////////////////////////////////////////////////////////////////////////////////

    gui_manager::gui_manager()
        : _about_dialog{ std::make_unique<about_dialog>() }
    {}
    gui_manager::~gui_manager() {
        if (this->is_initialized()) {
            this->deinitialize();
        }
    }

    bool gui_manager::is_initialized() const noexcept {
        return _flag_initialized;
    }

    void gui_manager::initialize(
        visualization::visualizer* const vis,
        const float dpi_scale_factor)
    {
        TRIENGINE_ASSERT(vis != nullptr);

        _vis = vis;

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
        const auto glctx = vis->get_gl_context();

        if (!::ImGui_ImplGlfw_InitForOpenGL(glctx->get_glfw_window(), true)) {
            TRIENGINE_PANIC("ImGui_ImplGlfw_InitForOpenGL failed");
        }

        if (!::ImGui_ImplOpenGL3_Init(glctx->get_shader_loader()->get_glsl_shader_version().c_str())) {
            TRIENGINE_PANIC("ImGui_ImplOpenGL3_Init failed");
        }

        this->setup_imgui_style();

        _flag_initialized = true;
        this->change_dpi_scale(dpi_scale_factor);

        _scene_window = std::make_shared<gui::scene_view_window>(_vis);

        // scene viewport is always docked to the central node (handled in setup_dock_space)
        _windows.push_back({ _scene_window, dock_slot::floating });
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

    void gui_manager::enable_main_menu(bool enable) { _flag_show_main_menu = enable; }
    bool gui_manager::is_main_menu_enabled() const { return _flag_show_main_menu; }

    std::shared_ptr<gui::scene_view_window> gui_manager::get_scene_window() const { return _scene_window; }

    void gui_manager::add_window(
        std::shared_ptr<iwindow> new_window,
        dock_slot slot)
    {
        if (!this->is_initialized()) {
            TRIENGINE_PANIC("Adding a GUI window requires an initialized GUI manager");
        }
        if (!new_window) {
            TRIENGINE_PANIC("Cannot add a null GUI window");
        }
        new_window->on_added(*_vis);
        _windows.push_back({ std::move(new_window), slot });
    }

    void gui_manager::render()
    {
        // Start the Dear ImGui frame
        ::ImGui_ImplOpenGL3_NewFrame();
        ::ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ImGui docking model primer:
        //   * A "dockspace" is a full-area container hosting dockable windows.
        //   * Inside it lives a tree of "dock nodes"; each leaf node can hold one or more
        //     windows shown as tabs.
        //   * A window (ImGui::Begin("Name", ...)) attaches to a node when its name matches
        //     the string previously bound via DockBuilderDockWindow() - i.e. windows and nodes
        //     are linked BY NAME, not by pointer/handle.
        //
        // Here we create (or reuse across frames) a top-level dockspace filling the OS viewport.
        // PassthruCentralNode keeps the central node transparent when nothing is docked in it,
        // so anything rendered to the OS framebuffer underneath stays visible.
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
                for (auto& entry : _windows) {
                    auto& window = entry.window;
                    bool p_selected = window->is_visible();
                    if (ImGui::MenuItem(window->get_window_name(), nullptr, &p_selected)) {
                        window->set_visible(p_selected);
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Info"))
            {
                if (ImGui::MenuItem("About Triengine...")) {
                    _about_dialog->request_open();
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
            for (auto& entry : _windows) {
                if (this->render_window(entry.window)) {
                    next_window_pos.x += scaled_padding_size;
                    next_window_pos.y += scaled_padding_size;
                    ImGui::SetNextWindowPos(next_window_pos, ImGuiCond_Once);
                }
            }
        }

        if (_about_dialog->is_visible()) {
            _about_dialog->render(_dpi_scale_factor);
        }

        // Build the initial dock layout exactly once on the first frame. After that, ImGui
        // retains the node tree and window-to-node bindings internally (and in imgui.ini when
        // enabled), so every subsequent frame just renders into the existing layout and lets
        // the user drag windows between nodes freely.
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
            window->on_pre_render(window_flags);

            if (ImGui::Begin(window->get_window_name(), &is_opened, window_flags)) {
                window_render_context render_ctx;
                render_ctx.dpi_scale = _dpi_scale_factor;
                render_ctx.visualizer = _vis;
                window->render(render_ctx);
            }

            // Always call a matching End() for each Begin() call, regardless of its return value!
            ImGui::End();
            window->on_post_render();
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
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.35f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.55f);
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

    void gui_manager::set_dock_split_ratios(const dock_split_ratios& ratios)
    {
        // ImGui's DockBuilderSplitNode silently tolerates bad ratios but produces ugly results;
        // clamp to a safe sub-range so user mistakes can't collapse a side leaf to zero or
        // flip the parent inside-out.
        constexpr float kMin = 0.05f;
        constexpr float kMax = 0.95f;
        _dock_ratios.left   = std::clamp(ratios.left,   kMin, kMax);
        _dock_ratios.right  = std::clamp(ratios.right,  kMin, kMax);
        _dock_ratios.top    = std::clamp(ratios.top,    kMin, kMax);
        _dock_ratios.bottom = std::clamp(ratios.bottom, kMin, kMax);
    }

    void gui_manager::setup_dock_space(
        const ImGuiID main_dockspace_id,
        ImGuiViewport* const viewport)
    {
        // The DockBuilder API (imgui_internal.h) lets us construct a dock node tree
        // programmatically instead of relying on the user to drag windows into place.
        // Typical flow:
        //   1. RemoveNode(id)                      - clear any prior tree registered under this id,
        //                                            including layout restored from imgui.ini. Without
        //                                            this, stale node state from previous runs bleeds in.
        //   2. AddNode(id, flags)                  - create a fresh empty root node. `flags` set its
        //                                            role: DockSpace marks it a top-level container,
        //                                            PassthruCentralNode makes the empty central leaf
        //                                            transparent, etc.
        //   3. SetNodeSize(id, size)               - required before SplitNode: split ratios are computed
        //                                            against the parent node's size, so a zero-sized
        //                                            root gives garbage child rects.
        //   4. SplitNode(...) repeatedly           - split a node into two child nodes along a direction.
        //                                            The parent becomes an internal branch; only unsplit
        //                                            leaves can host docked windows.
        //   5. DockWindow("window name", node_id)  - reserve a binding: "next time a window calls
        //                                            ImGui::Begin() with this exact name string, place
        //                                            it into node_id". Match is by name only - one
        //                                            character off and the window pops out floating.
        //   6. Finish(id)                          - atomically commit every pending builder op from
        //                                            steps 1-5. Forgetting this is the #1 mistake:
        //                                            the tree exists in a staging area and never goes
        //                                            live, so the UI shows blank or the previous layout.
        // All builder operations mutate a "pending" layout; nothing is visible until Finish().
        //
        // Refs:
        // https://github.com/ocornut/imgui/issues/4430
        // https://gist.github.com/moebiussurfing/d7e6ec46a44985dd557d7678ddfeda99
        // https://github.com/moebiussurfing/ofxSurfingImGui/blob/master/3_Docking/3_0_Layout_Docking2/src/ofApp.cpp#L324-L361

        // split ratios (user-configurable via gui_manager::set_dock_split_ratios)
        const float kSplitRatioLeft   = _dock_ratios.left;
        const float kSplitRatioRight  = _dock_ratios.right;
        const float kSplitRatioTop    = _dock_ratios.top;
        const float kSplitRatioBottom = _dock_ratios.bottom;

        // Wipe any prior layout for this dockspace id and recreate an empty root node
        // flagged as a DockSpace (top-level container) with a transparent central leaf.
        ImGui::DockBuilderRemoveNode(main_dockspace_id);
        ImGui::DockBuilderAddNode(main_dockspace_id, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(main_dockspace_id, viewport->Size);

        // DockBuilderSplitNode signature:
        //   ImGuiID SplitNode(
        //       ImGuiID  node_id,                    // node to split in two
        //       ImGuiDir split_dir,                  // side the new child goes to (Left/Right/Up/Down)
        //       float    size_ratio_for_node_at_dir, // ratio 0..1 for the `at_dir` child (NOT the opposite)
        //       ImGuiID* out_id_at_dir,              // [out, optional] child id on split_dir side
        //       ImGuiID* out_id_at_opposite_dir      // [out, optional] child id on the opposite side
        //   );
        //   // return value == *out_id_at_dir - one of the two outputs can be taken as return.
        //
        // After SplitNode, `node_id` stops being a leaf and becomes an internal container
        // holding two new child leaves. Only the child ids (from the out-params / return value)
        // are valid DockWindow targets; docking onto the old node_id does nothing useful.
        //
        // Below we reuse a single `dock_id_center` as the opposite-side out-param across four
        // calls. Each call:
        //   (1) splits the CURRENT center node into a side leaf + a smaller remainder,
        //   (2) captures the side leaf via the return value (-> dock_id_left/right/top/bottom),
        //   (3) OVERWRITES dock_id_center with the id of the smaller remainder.
        // So `dock_id_center` walks down the tree, each step naming a strictly smaller central
        // region. After all four splits it refers to the final central rectangle in the middle
        // - this is where the 3D scene viewport docks.
        //
        //   initial            after Left         after Right        after Up           after Down
        //   +----------+       +---+------+       +---+----+--+      +---+----+--+      +---+----+--+
        //   |          |       |   |      |       |   |    |  |      |   | top|  |      |   | top|  |
        //   |  center  |  -->  | L |center|  -->  | L |ctr | R|  ->  | L +----+ R|  ->  | L +----+ R|
        //   |          |       |   |      |       |   |    |  |      |   | ctr|  |      |   | ctr|  |
        //   +----------+       +---+------+       +---+----+--+      +---+----+0-+      |   +----+  |
        //                                                                               |   | bot|  |
        //                                                                               +---+----+--+
        //
        // Gotcha (3rd argument - `size_ratio_for_node_at_dir`):
        // the ratio names the `at_dir` child (the new side leaf being split off), NOT the
        // remainder/center. So kSplitRatioLeft = 0.22 with ImGuiDir_Left means the LEFT
        // child takes 22% of the parent and the remainder (78%) becomes the new center.
        // A common bug is thinking "I want center to stay at 78%" and passing 0.78 here,
        // which actually produces a giant 78%-wide side leaf and a tiny 22% center.
        ImGuiID dock_id_center = main_dockspace_id;
        ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_id_center, ImGuiDir_Left, kSplitRatioLeft, nullptr, &dock_id_center);
        ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_id_center, ImGuiDir_Right, kSplitRatioRight, nullptr, &dock_id_center);
        ImGuiID dock_id_top = ImGui::DockBuilderSplitNode(dock_id_center, ImGuiDir_Up, kSplitRatioTop, nullptr, &dock_id_center);
        ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_id_center, ImGuiDir_Down, kSplitRatioBottom, nullptr, &dock_id_center);

        // DockBuilderDockWindow binds a window to a node using the window's name string.
        // When ImGui::Begin("<name>", ...) runs later in the frame, ImGui looks up that
        // binding and places the window into the matching node. The name MUST match
        // exactly what iwindow::get_window_name() returns.
        //
        // Scene viewport always occupies the central (leftover) node.
        ImGui::DockBuilderDockWindow(_scene_window->get_window_name(), dock_id_center);

        // dock user-registered windows according to their slot selection
        for (auto& entry : _windows) {
            if (entry.window == _scene_window) {
                continue; // already docked above
            }
            ImGuiID target_id = 0;
            switch (entry.slot) {
            case dock_slot::left:   target_id = dock_id_left;   break;
            case dock_slot::right:  target_id = dock_id_right;  break;
            case dock_slot::top:    target_id = dock_id_top;    break;
            case dock_slot::bottom: target_id = dock_id_bottom; break;
            case dock_slot::floating:
            default:
                // No DockBuilderDockWindow call = the window is unbound and starts as a
                // free-floating window. The user can still drag it onto a node later.
                continue;
            }
            ImGui::DockBuilderDockWindow(entry.window->get_window_name(), target_id);
        }

        // Commit the pending tree; after this the layout is live and windows begin
        // landing in their assigned nodes on their next ImGui::Begin() call.
        ImGui::DockBuilderFinish(main_dockspace_id);
    }

} // namespace