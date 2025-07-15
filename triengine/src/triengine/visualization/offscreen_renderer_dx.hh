#pragma once
#include <triengine/common.h>
#if !defined(_TRIENGINE_PLATFORM_WIN32)
#  error Unsupported platform
#endif // ^^^ _TRIENGINE_PLATFORM_WIN32 ^^^

#include <triengine/core/gl_context.hh>
#include <triengine/core/scene_renderer.hh>
#include <triengine/utility/noncopyable.hh>

#define INITGUID
#include <windows.h>
#include <wrl/client.h> // Microsoft::WRL::ComPtr
#include <dxgi1_2.h> // DXGI 1.2 API header
#include <d3d11_2.h> // DX11.2 API header
#include <d3dcompiler.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <functional>
#include <unordered_map>
#include <array>

namespace triengine::visualization
{
    class offscreen_renderer_dx
        : utility::noncopyable
    {
    public:
        offscreen_renderer_dx();
        virtual ~offscreen_renderer_dx() = default;

        const core::gl_context* get_gl_context() const noexcept;
        core::gl_context* get_gl_context() noexcept;

        void create_renderer(
            Microsoft::WRL::ComPtr<ID3D11Device2> dx11_device2,
            Microsoft::WRL::ComPtr<ID3D11DeviceContext2> dx11_device_context2,
            int32_t frame_width,
            int32_t frame_height
        );

        void destroy_renderer();

        std::shared_ptr<scene> add_scene();
        void remove_scene(scene_id_t scn_id);

        void switch_scene(scene_id_t scn_id);
        void switch_to_previous_scene();
        void switch_to_next_scene();

        std::shared_ptr<const scene> find_scene(scene_id_t scn_id) const;
        std::shared_ptr<scene> find_scene(scene_id_t scn_id);

        std::shared_ptr<const scene> get_current_scene() const;
        std::shared_ptr<scene> get_current_scene();

        vec2_i32 get_frame_size() const noexcept;
        void resize_frame(int32_t width, int32_t height);

        ID3D11Texture2D* render();

    private:
        void _begin_frame();
        void _end_frame();
        void _handle_frame_resize_event(int32_t width, int32_t height);

    private:
        bool _flag_initialized{ false };

        core::gl_context _glctx;
        vec2_i32 _curr_frame_size{};

        core::scene_renderer _scn_renderer;

        std::list<std::shared_ptr<scene>> _scn_list;
        std::list<std::shared_ptr<scene>>::iterator _curr_scn_it{ _scn_list.end() };
        std::unordered_map<
            scene_id_t,
            std::list<std::shared_ptr<scene>>::iterator
        > _scn_id_map;

        // DX Resources
        Microsoft::WRL::ComPtr<ID3D11Device2> _dx11_device2;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext2> _dx11_device_context2;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> _dx11_gl_interop_color_texture;

        // WGL DX Interop Resources
        std::shared_ptr<std::remove_pointer_t<HANDLE>> _wgl_dx11_device_handle;
        std::shared_ptr<std::remove_pointer_t<HANDLE>> _wgl_dx11_gl_interop_texture_handle;

        GLuint _main_fbo{}; // Framebuffer Object ID
        GLuint _frame_gl_interop_color_texture{}; // Texture ID

    }; // class

} // namespace