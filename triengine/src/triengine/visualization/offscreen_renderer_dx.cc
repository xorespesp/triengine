#include "offscreen_renderer_dx.hh"
#include <triengine/utility/bit_cast.hh>
#include <triengine/utility/string_format.hh>
#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>
#include <triengine/utility/logger.hh>

#include <iostream>
#include <memory>

// https://www.opengl.org/registry/api/GL/wglext.h
#include <triengine/extern/wglext.h>

#define ASSERT_HR(EXPR) assert_hr_impl(EXPR, _TRIENGINE_CURRENT_SOURCE_LOC())

using Microsoft::WRL::ComPtr;

static PFNWGLDXOPENDEVICENVPROC wglDXOpenDeviceNV{ nullptr };
static PFNWGLDXCLOSEDEVICENVPROC wglDXCloseDeviceNV{ nullptr };
static PFNWGLDXREGISTEROBJECTNVPROC wglDXRegisterObjectNV{ nullptr };
static PFNWGLDXUNREGISTEROBJECTNVPROC wglDXUnregisterObjectNV{ nullptr };
static PFNWGLDXLOCKOBJECTSNVPROC wglDXLockObjectsNV{ nullptr };
static PFNWGLDXUNLOCKOBJECTSNVPROC wglDXUnlockObjectsNV{ nullptr };

namespace triengine::visualization
{
    namespace
    {
        inline void assert_hr_impl(
            const HRESULT hr,
            const utility::source_loc& loc)
        {
            if (FAILED(hr)) {
                const std::string_view src_file_name{ loc.filename() };
                throw std::runtime_error{ utility::string::c_format("[ERROR] HRESULT failed with code 0x%08X at %.*s:%u"
                    , hr
                    , static_cast<int>(src_file_name.size())
                    , src_file_name.data()
                    , loc.line)
                };
            }
        }

        template <typename _FunPtr>
        _FunPtr load_gl_extension_funptr(const char* const func_name)
        {
            static_assert(std::is_pointer_v<_FunPtr> && std::is_function_v<std::remove_pointer_t<_FunPtr>>, "!!");
            // NOTE: When no current rendering context exists or the function fails, the return value is NULL.
            const PROC funptr{ ::wglGetProcAddress(func_name) };
            if (!funptr) { TRIENGINE_PANIC("Failed to load GL extension funptr: %s (last error: %u)", func_name, ::GetLastError()); }
            return utility::bit_cast<_FunPtr>(funptr);
        }

        void load_wgl_nvdx_interop_functions()
        {
            wglDXOpenDeviceNV = load_gl_extension_funptr<decltype(wglDXOpenDeviceNV)>("wglDXOpenDeviceNV");
            wglDXCloseDeviceNV = load_gl_extension_funptr<decltype(wglDXCloseDeviceNV)>("wglDXCloseDeviceNV");
            wglDXRegisterObjectNV = load_gl_extension_funptr<decltype(wglDXRegisterObjectNV)>("wglDXRegisterObjectNV");
            wglDXUnregisterObjectNV = load_gl_extension_funptr<decltype(wglDXUnregisterObjectNV)>("wglDXUnregisterObjectNV");
            wglDXLockObjectsNV = load_gl_extension_funptr<decltype(wglDXLockObjectsNV)>("wglDXLockObjectsNV");
            wglDXUnlockObjectsNV = load_gl_extension_funptr<decltype(wglDXUnlockObjectsNV)>("wglDXUnlockObjectsNV");
        }

        ComPtr<ID3D11Texture2D> create_dxgl_interop_texture(
            const uint32_t width_pixels,
            const uint32_t height_pixels,
            ComPtr<ID3D11Device2> device)
        {
            ComPtr<ID3D11Texture2D> dxgl_interop_texture;

            D3D11_TEXTURE2D_DESC dxgl_interop_texture_desc = { 0, };
            dxgl_interop_texture_desc.Width = width_pixels;
            dxgl_interop_texture_desc.Height = height_pixels;
            dxgl_interop_texture_desc.MipLevels = 1;
            dxgl_interop_texture_desc.ArraySize = 1;
            dxgl_interop_texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            dxgl_interop_texture_desc.SampleDesc.Count = 1;
            dxgl_interop_texture_desc.SampleDesc.Quality = 0;
            dxgl_interop_texture_desc.Usage = D3D11_USAGE_DEFAULT; // NOTE: GL Interop용 텍스처는 Usage 플래그가 반드시 D3D11_USAGE_DEFAULT여야 함 (See: https://registry.khronos.org/OpenGL/extensions/NV/WGL_NV_DX_interop2.txt)
            dxgl_interop_texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            dxgl_interop_texture_desc.CPUAccessFlags = 0;
            dxgl_interop_texture_desc.MiscFlags = 0; // GL Interop용 텍스처는 공유 속성이 필요 없음
            ASSERT_HR(device->CreateTexture2D(&dxgl_interop_texture_desc, NULL, &dxgl_interop_texture));

            return dxgl_interop_texture;
        }

    } // namespace

    offscreen_renderer_dx::offscreen_renderer_dx()
    {
    }

    const core::gl_context* offscreen_renderer_dx::get_gl_context() const noexcept
    {
        return &_glctx;
    }

    core::gl_context* offscreen_renderer_dx::get_gl_context() noexcept
    {
        return &_glctx;
    }

    void offscreen_renderer_dx::create_renderer(
        Microsoft::WRL::ComPtr<ID3D11Device2> dx11_device2,
        Microsoft::WRL::ComPtr<ID3D11DeviceContext2> dx11_device_context2,
        int32_t frame_width,
        int32_t frame_height)
    {
        if (_flag_initialized) {
            TRIENGINE_PANIC("already created");
        }

        _glctx.create(
            "",
            false,
            frame_width,
            frame_height,
            false
        );

        // NOTE: Must be called after the OpenGL rendering context has been created.
        load_wgl_nvdx_interop_functions();

        _glctx.set_frame_resize_callback(std::bind(&offscreen_renderer_dx::_handle_frame_resize_event, this, 
            std::placeholders::_1, std::placeholders::_2));

        _curr_frame_size = _glctx.get_window_size();

        _scn_renderer.create(&_glctx);

        _dx11_device2 = dx11_device2;
        _dx11_device_context2 = dx11_device_context2;

        _dx11_gl_interop_color_texture = create_dxgl_interop_texture(
            frame_width,
            frame_height,
            _dx11_device2
        );

        // DX Device를 OpenGL Interop용으로 Open
        _wgl_dx11_device_handle.reset(
            ::wglDXOpenDeviceNV(_dx11_device2.Get()),
            [](HANDLE hDevice) { if (hDevice) { ::wglDXCloseDeviceNV(hDevice); } }
        );
        if (!_wgl_dx11_device_handle) {
            TRIENGINE_PANIC("wglDXOpenDeviceNV failed");
        }

        // FBO에 붙일 컬러 텍스처를 생성하고, 이를 WGL DX Interop 텍스처(비공유 텍스처)로 등록
        ::glCreateTextures(GL_TEXTURE_2D, 1, &_frame_gl_interop_color_texture);
        _wgl_dx11_gl_interop_texture_handle.reset(
            ::wglDXRegisterObjectNV(
                _wgl_dx11_device_handle.get()/* HANDLE hDevice; */,
                _dx11_gl_interop_color_texture.Get()/* PVOID dxResource; */,
                _frame_gl_interop_color_texture/* GLuint name; */,
                GL_TEXTURE_2D/* GLenum type; */,
                WGL_ACCESS_READ_WRITE_NV/* GLenum access; */
            ),
            [this](HANDLE hObject) { if (hObject) { ::wglDXUnregisterObjectNV(_wgl_dx11_device_handle.get(), hObject); } }
        );

        // DSA 방식으로 FBO 생성
        ::glCreateFramebuffers(1, &_main_fbo);
        ::glNamedFramebufferTexture(_main_fbo, GL_COLOR_ATTACHMENT0, _frame_gl_interop_color_texture, 0); // FBO에 컬러 텍스처(WGL DX Interop 텍스처) 부착
        if (GL_FRAMEBUFFER_COMPLETE != ::glCheckNamedFramebufferStatus(_main_fbo, GL_FRAMEBUFFER)) {
            TRIENGINE_PANIC("Framebuffer is not complete!");
        }

        TRIENGINE_TRACE("%s() LEAVE", __func__);
        _flag_initialized = true;
    }

    void offscreen_renderer_dx::destroy_renderer()
    {
        if (_flag_initialized)
        {
            _flag_initialized = false;

            _wgl_dx11_gl_interop_texture_handle.reset();
            _wgl_dx11_device_handle.reset();

            if (_frame_gl_interop_color_texture) {
                ::glDeleteTextures(1, &_frame_gl_interop_color_texture);
            }

            if (_main_fbo) {
                ::glDeleteFramebuffers(1, &_main_fbo);
            }

            _scn_renderer.destroy();
            _glctx.destroy();
        }
    }

    std::shared_ptr<scene> offscreen_renderer_dx::add_scene()
    {
        auto new_scn = std::make_shared<scene>(_glctx.get_gpu_resource_manager());
        if (_scn_id_map.count(new_scn->get_id())) {
            TRIENGINE_PANIC("Failed to add scene (id #%X already exists)", new_scn->get_id());
        }

        const bool is_first{ _scn_list.empty() };

        _scn_list.push_back(new_scn);
        _scn_id_map[new_scn->get_id()] = std::prev(_scn_list.end());

        if (is_first) {
            _curr_scn_it = std::prev(_scn_list.end());
        }

        return new_scn;
    }

    void offscreen_renderer_dx::remove_scene(scene_id_t scn_id)
    {
        auto map_it = _scn_id_map.find(scn_id);
        if (map_it != _scn_id_map.end()) {
            _scn_list.erase(map_it->second);
            _scn_id_map.erase(map_it);
            if ((*_curr_scn_it)->get_id() == scn_id) {
                _curr_scn_it = _scn_id_map.empty()
                    ? _scn_list.end()
                    : _scn_list.begin();
            }
        }
        else {
            TRIENGINE_WARN("Failed to remove scene #%X (not found)", scn_id);
        }
    }

    void offscreen_renderer_dx::switch_scene(scene_id_t scn_id)
    {
        auto map_it = _scn_id_map.find(scn_id);
        if (map_it == _scn_id_map.end()) {
            TRIENGINE_PANIC("Failed to change scene (invalid scene id #%X)", scn_id);
        }
        _curr_scn_it = map_it->second;
    }

    void offscreen_renderer_dx::switch_to_previous_scene()
    {
        if (_curr_scn_it != _scn_list.end()) {
            _curr_scn_it = std::prev((_curr_scn_it != _scn_list.begin())
                ? _curr_scn_it
                : _scn_list.end()
            );
        }
    }

    void offscreen_renderer_dx::switch_to_next_scene()
    {
        if (_curr_scn_it != _scn_list.end()) {
            const auto next_it = std::next(_curr_scn_it);
            _curr_scn_it = (next_it != _scn_list.end())
                ? next_it
                : _scn_list.begin();
        }
    }

    std::shared_ptr<const scene> offscreen_renderer_dx::find_scene(scene_id_t scn_id) const
    {
        auto map_it = _scn_id_map.find(scn_id);
        return (map_it != _scn_id_map.end())
            ? *(map_it->second)
            : nullptr;
    }

    std::shared_ptr<scene> offscreen_renderer_dx::find_scene(scene_id_t scn_id)
    {
        auto map_it = _scn_id_map.find(scn_id);
        return (map_it != _scn_id_map.end())
            ? *(map_it->second)
            : nullptr;
    }

    std::shared_ptr<const scene> offscreen_renderer_dx::get_current_scene() const
    {
        return (_curr_scn_it != _scn_list.end())
            ? *_curr_scn_it
            : nullptr;
    }

    std::shared_ptr<scene> offscreen_renderer_dx::get_current_scene()
    {
        return (_curr_scn_it != _scn_list.end())
            ? *_curr_scn_it
            : nullptr;
    }

    vec2_i32 offscreen_renderer_dx::get_frame_size() const noexcept
    {
        return _curr_frame_size;
    }

    void offscreen_renderer_dx::resize_frame(
        const int32_t width,
        const int32_t height)
    {
        if (width <= 0 || height <= 0) {
            TRIENGINE_PANIC("Invalid frame size (%d, %d)", width, height);
        }

        if (_curr_frame_size != vec2_i32{ width, height })
        {
            ::glfwSetWindowSize(
                _glctx.get_glfw_window(),
                width,
                height
            );
        }
    }

    ID3D11Texture2D* offscreen_renderer_dx::render()
    {
        if (_curr_scn_it == _scn_list.end()) {
            TRIENGINE_PANIC("No scenes added");
            return nullptr;
        }

        _glctx.swap_buffers();
        _glctx.poll_window_events();

        const vec2_i32 frame_size = _curr_frame_size;

        this->_begin_frame();
        {
            scene& target_scn = *(_curr_scn_it->get());
            target_scn.get_camera()->set_view_port(view_port{ 0, 0, frame_size.x(),  frame_size.y() });
            _scn_renderer.render(
                _main_fbo,
                _curr_frame_size.x(),
                _curr_frame_size.y(),
                target_scn
            );
        }
        this->_end_frame();

        return _dx11_gl_interop_color_texture.Get();
    }

    void offscreen_renderer_dx::_begin_frame()
    {
        HANDLE handle_value = _wgl_dx11_gl_interop_texture_handle.get();
        ::wglDXLockObjectsNV(_wgl_dx11_device_handle.get(), 1, &handle_value);
    }

    void offscreen_renderer_dx::_end_frame()
    {
        HANDLE handle_value = _wgl_dx11_gl_interop_texture_handle.get();
        ::wglDXUnlockObjectsNV(_wgl_dx11_device_handle.get(), 1, &handle_value);
    }

    void offscreen_renderer_dx::_handle_frame_resize_event(
        [[maybe_unused]] const int32_t new_frame_width,
        [[maybe_unused]] const int32_t new_frame_height)
    {
        if (_curr_frame_size == vec2_i32{ new_frame_width, new_frame_height }) {
            return; // skip resize
        }

        // Recreate gldx interop color texture
        _dx11_gl_interop_color_texture = create_dxgl_interop_texture(new_frame_width, new_frame_height, _dx11_device2);
        TRIENGINE_ASSERT(_dx11_gl_interop_color_texture != nullptr);

        TRIENGINE_ASSERT(_frame_gl_interop_color_texture != 0);
        ::glDeleteTextures(1, &_frame_gl_interop_color_texture);
        ::glCreateTextures(GL_TEXTURE_2D, 1, &_frame_gl_interop_color_texture);

        _wgl_dx11_gl_interop_texture_handle.reset(
            ::wglDXRegisterObjectNV(
                _wgl_dx11_device_handle.get()/* HANDLE hDevice; */,
                _dx11_gl_interop_color_texture.Get()/* PVOID dxResource; */,
                _frame_gl_interop_color_texture/* GLuint name; */,
                GL_TEXTURE_2D/* GLenum type; */,
                WGL_ACCESS_READ_WRITE_NV/* GLenum access; */
            ),
            [this](HANDLE hObject) { if (hObject) { ::wglDXUnregisterObjectNV(_wgl_dx11_device_handle.get(), hObject); } }
        );
        TRIENGINE_ASSERT(_wgl_dx11_gl_interop_texture_handle != nullptr);

        ::glNamedFramebufferTexture(_main_fbo, GL_COLOR_ATTACHMENT0, _frame_gl_interop_color_texture, 0); // FBO에 컬러 텍스처(WGL DX Interop 텍스처) 부착
        if (GL_FRAMEBUFFER_COMPLETE != ::glCheckNamedFramebufferStatus(_main_fbo, GL_FRAMEBUFFER)) {
            TRIENGINE_PANIC("Framebuffer is not complete!");
        }

        _curr_frame_size = vec2_i32{ new_frame_width, new_frame_height };
    }

} // namespace
