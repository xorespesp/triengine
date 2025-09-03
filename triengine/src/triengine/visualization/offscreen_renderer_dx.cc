#include "offscreen_renderer_dx.hh"

#include <d3dcompiler.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <triengine/utility/bit_cast.hh>
#include <triengine/utility/string_format.hh>
#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/gl_utils.hh>
#include <triengine/utility/logger.hh>

#include <iostream>
#include <memory>

#define ASSERT_HR(EXPR) \
    assert_hr_impl(EXPR, _TRIENGINE_CURRENT_SOURCE_LOC())

#define THROW_IF_FAILED(hr_expr) \
    if (const HRESULT __expr_hr__{ hr_expr }; FAILED(__expr_hr__)) { \
        ::triengine::utility::panicf_impl( \
            _TRIENGINE_CURRENT_SOURCE_LOC(), \
            "HRESULT failed with code 0x{:X}", \
            static_cast<uint32_t>(__expr_hr__) \
        ); \
    }

using Microsoft::WRL::ComPtr;

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

    } // namespace

    offscreen_renderer_dx::offscreen_renderer_dx()
    { }

    offscreen_renderer_dx::~offscreen_renderer_dx()
    { }

    const core::gl_context* offscreen_renderer_dx::get_gl_context() const noexcept
    {
        TRIENGINE_ASSERT(_flag_initialized);
        return &_glctx;
    }

    core::gl_context* offscreen_renderer_dx::get_gl_context() noexcept
    {
        TRIENGINE_ASSERT(_flag_initialized);
        return &_glctx;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> offscreen_renderer_dx::get_dxgi_adapter() const noexcept
    {
        TRIENGINE_ASSERT(_flag_initialized);
        TRIENGINE_ASSERT(_dxgi_adapter != nullptr);
        return _dxgi_adapter;
    }

    Microsoft::WRL::ComPtr<ID3D11Device2> offscreen_renderer_dx::get_dx11_device() const noexcept
    {
        TRIENGINE_ASSERT(_flag_initialized);
        TRIENGINE_ASSERT(_dx11_device2 != nullptr);
        return _dx11_device2;
    }

    Microsoft::WRL::ComPtr<ID3D11DeviceContext2> offscreen_renderer_dx::get_dx11_device_context() const noexcept
    {
        TRIENGINE_ASSERT(_flag_initialized);
        TRIENGINE_ASSERT(_dx11_device_context2 != nullptr);
        return _dx11_device_context2;
    }

    vec2_i32 offscreen_renderer_dx::get_frame_size() const noexcept
    {
        TRIENGINE_ASSERT(_flag_initialized);
        return _curr_frame_size;
    }

    shared_win32_handle offscreen_renderer_dx::get_surface_handle() const
    {
        TRIENGINE_ASSERT(_flag_initialized);
        TRIENGINE_ASSERT(_dx11_interop_color_tex_handle != nullptr);
        return _dx11_interop_color_tex_handle;
    }

    void offscreen_renderer_dx::create_renderer(const vec2_i32 initial_frame_size)
    {
        TRIENGINE_DEBUG("Creating DX offscreen renderer with frame size %dx%d"
            , initial_frame_size.x()
            , initial_frame_size.y()
        );

        if (initial_frame_size.x() <= 0 || initial_frame_size.y() <= 0) {
            TRIENGINE_PANIC("Invalid frame size: %dx%d", initial_frame_size.x(), initial_frame_size.y());
        }

        if (_flag_initialized) {
            TRIENGINE_PANIC("already created");
        }

        _glctx.create(
            "",
            false, // Disable window visibility
            initial_frame_size.x(),
            initial_frame_size.y(),
            false, // Disable fullscreen
            false // Disable VSync
        );

        TRIENGINE_DEBUG("Checking OpenGL compatibility...");

        const std::string_view
            gl_version{ reinterpret_cast<const char*>(::glGetString(GL_VERSION)) },
            gl_vendor_name{ reinterpret_cast<const char*>(::glGetString(GL_VENDOR)) },
            gl_renderer_name{ reinterpret_cast<const char*>(::glGetString(GL_RENDERER)) };

        TRIENGINE_TRACE("GL Version: %.*s", static_cast<int>(gl_version.size()), gl_version.data());
        TRIENGINE_TRACE("GL Vendor: %.*s", static_cast<int>(gl_vendor_name.size()), gl_vendor_name.data());
        TRIENGINE_TRACE("GL Renderer: %.*s", static_cast<int>(gl_renderer_name.size()), gl_renderer_name.data());

        bool hasExternalObjects = false;
        bool hasExternalObjectsWin32 = false;
        bool hasWin32KeyedMutex = false;
        
        GLint numExtensions{};
        ::glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
        for (GLint i = 0; i < numExtensions; ++i) {
            const std::string_view gl_extension{ reinterpret_cast<const char*>(::glGetStringi(GL_EXTENSIONS, i)) };
            if (gl_extension == "GL_EXT_memory_object") {
                hasExternalObjects = true;
            } else if (gl_extension == "GL_EXT_memory_object_win32") {
                hasExternalObjectsWin32 = true;
            } else if (gl_extension == "GL_EXT_win32_keyed_mutex") {
                hasWin32KeyedMutex = true;
            }
        }

        TRIENGINE_TRACE("GL_EXT_memory_object: %s", hasExternalObjects ? "Y" : "N");
        TRIENGINE_TRACE("GL_EXT_memory_object_win32: %s", hasExternalObjectsWin32 ? "Y" : "N");
        TRIENGINE_TRACE("GL_EXT_win32_keyed_mutex: %s", hasWin32KeyedMutex ? "Y" : "N");

        if (!hasExternalObjects || !hasExternalObjectsWin32 || !hasWin32KeyedMutex) {
            TRIENGINE_PANIC(
                "Required OpenGL extensions are not supported by this GPU/driver. "
                "EXT_memory_object, EXT_memory_object_win32, GL_EXT_win32_keyed_mutex support is required. "
                "Please use a modern GPU with updated drivers that support these extensions."
            );
        }

        //
        // Initialize DX11 context
        //
        
        // NOTE: 반드시 CreateDXGIFactory2 함수를 사용해서 DXGI 1.2 버전 이상의 DXGI 팩토리(`IDXGIFactory`)를 생성해줘야 함.
        // (`ID3D11Device::CreateTexture2D: D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX is only available for devices created off of Dxgi1.1 factories or later.` D3D11 오류 방지)
        ComPtr<IDXGIFactory2> dxgi_factory2;
        if (HRESULT hr = ::CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgi_factory2));
            FAILED(hr)) {
            TRIENGINE_PANIC("Failed to create DXGI factory (HRESULT: 0x%X)", hr);
        }

        //
        // find DXGI adapter, based on OpenGL renderer name
        //

        const std::wstring target_adapter_name = []() -> std::wstring
        {
            const GLubyte* const gl_renderer_name = ::glGetString(GL_RENDERER);
            if (!gl_renderer_name) {
                TRIENGINE_PANIC("Failed to get OpenGL renderer name");
            }

            // Convert OpenGL renderer name to wide string use MBCS
            const std::string gl_renderer_name_str{ reinterpret_cast<const char*>(gl_renderer_name) };
            const int wide_str_size = ::MultiByteToWideChar(
                CP_ACP, 
                0, 
                gl_renderer_name_str.c_str(), 
                -1, 
                nullptr, 
                0
            );
            if (wide_str_size <= 0) {
                TRIENGINE_PANIC("Failed to convert OpenGL renderer name to wide string (last error: %u)", ::GetLastError());
            }

            std::wstring target_adapter_name(wide_str_size, L'\0');
            const int result = ::MultiByteToWideChar(
                CP_ACP, 
                0, 
                gl_renderer_name_str.c_str(), 
                -1,
                target_adapter_name.data(), 
                wide_str_size
            );
            if (result <= 0) {
                TRIENGINE_PANIC("Failed to convert OpenGL renderer name to wide string (last error: %u)", ::GetLastError());
            }

            target_adapter_name.resize(wide_str_size - 1); // Remove null terminator
            return target_adapter_name;
        }();

        TRIENGINE_DEBUG("Target adapter name: %.*S"
            , static_cast<int>(target_adapter_name.size())
            , target_adapter_name.data()
        );
        ComPtr<IDXGIAdapter> selected_adapter0;
        bool found_matching_adapter = false;

        TRIENGINE_DEBUG("Enumerating DXGI adapters...");
        for (UINT curr_adapter_index = 0; ; ++curr_adapter_index)
        {
            ComPtr<IDXGIAdapter> curr_adapter;
            if (HRESULT hr = dxgi_factory2->EnumAdapters(curr_adapter_index, &curr_adapter);
                hr == DXGI_ERROR_NOT_FOUND)
            {
                // No more adapters available, exit the loop
                break;
            }
            else if (FAILED(hr))
            {
                TRIENGINE_WARN("Failed to enumerate adapter #%u (HRESULT 0x%X)", curr_adapter_index, hr);
                continue;
            }

            DXGI_ADAPTER_DESC curr_adapter_desc;
            if (HRESULT hr = curr_adapter->GetDesc(&curr_adapter_desc);
                FAILED(hr))
            {
                TRIENGINE_WARN("Failed to get description for adapter #%u (HRESULT 0x%X)", curr_adapter_index, hr);
                continue;
            }

            std::wstring_view curr_adapter_name{ curr_adapter_desc.Description };
            TRIENGINE_TRACE("Enumerate adapter #%u: %.*S"
                , curr_adapter_index
                , static_cast<int>(curr_adapter_name.size())
                , curr_adapter_name.data()
            );

            const bool name_matches = curr_adapter_name.length() >= target_adapter_name.length() 
                ? (curr_adapter_name.find(target_adapter_name) != std::wstring_view::npos)
                : (target_adapter_name.find(curr_adapter_name) != std::wstring_view::npos);

            if (name_matches) {
                selected_adapter0 = curr_adapter;
                found_matching_adapter = true;
                TRIENGINE_DEBUG("Found matching adapter!");
                break;
            }
        } // for

        // 타겟 어댑터를 찾지 못한 경우 기본 어댑터(0번) 사용
        if (!found_matching_adapter)
        {
            TRIENGINE_WARN("Target adapter not found, using default adapter (index 0)");
            if (HRESULT hr = dxgi_factory2->EnumAdapters(0, &selected_adapter0);
                FAILED(hr)) {
                TRIENGINE_PANIC("Failed to get default adapter (HRESULT: 0x%X)", hr);
            }
        }

        _dxgi_adapter = selected_adapter0;

        ComPtr<ID3D11Device> dx11_device0;
        ComPtr<ID3D11DeviceContext> dx11_device_context0;

        UINT device_creation_flags = 0;
#if defined(TRIENGINE_DEBUG)
        device_creation_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // ^^^ TRIENGINE_DEBUG ^^^

        if (HRESULT hr = ::D3D11CreateDevice(
            selected_adapter0.Get(),
            D3D_DRIVER_TYPE_UNKNOWN,
            nullptr,
            device_creation_flags,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &dx11_device0,
            nullptr,
            &dx11_device_context0
        ); FAILED(hr)) {
            TRIENGINE_PANIC("D3D11CreateDevice Failed (HRESULT: 0x%X)", hr);
        }

        // Convert `ID3D11Device` -> `ID3D11Device2` (Higher version object)
        ASSERT_HR(dx11_device0.As(&_dx11_device2));

        // Convert `ID3D11DeviceContext` -> `ID3D11DeviceContext2` (Higher version object)
        ASSERT_HR(dx11_device_context0.As(&_dx11_device_context2));

        if (!_dx11_device2) {
            TRIENGINE_PANIC("Failed to create DX11 device2");
        }

        if (!_dx11_device_context2) {
            TRIENGINE_PANIC("Failed to create DX11 device context2");
        }

        // Create FBO
        ::glCreateFramebuffers(1, &_gl_fbo);

        _glctx.set_frame_resize_callback(std::bind(&offscreen_renderer_dx::_resize_frame, this,
            std::placeholders::_1));

        // Initil resize to create rest...
        this->_resize_frame(initial_frame_size);

        _scn_renderer.create(&_glctx);

        TRIENGINE_TRACE("%s() LEAVE", __func__);
        _flag_initialized = true;
    }

    void offscreen_renderer_dx::destroy_renderer()
    {
        if (_flag_initialized)
        {
            _flag_initialized = false;

            // Clear D3D11 device context state if available
            if (_dx11_device_context2) {
                _dx11_device_context2->ClearState();
                _dx11_device_context2->Flush();
            }

            // Clean up EXT_external_objects resources first
            if (_gl_interop_color_tex_mem_object) {
                ::glDeleteMemoryObjectsEXT(1, &_gl_interop_color_tex_mem_object);
                _gl_interop_color_tex_mem_object = 0;
            }

            // Clean up shared texture handle
            _dx11_interop_color_tex_handle.reset();

            // Clean up OpenGL resources
            if (_gl_fbo) {
                ::glDeleteFramebuffers(1, &_gl_fbo);
                _gl_fbo = 0;
            }

            if (_gl_interop_color_tex) {
                ::glDeleteTextures(1, &_gl_interop_color_tex);
                _gl_interop_color_tex = 0;
            }

            // Clean up D3D11 resources (COM objects will auto-release)
            _dx11_interop_color_tex.Reset();
            _dx11_device_context2.Reset();
            _dx11_device2.Reset();
            _dxgi_adapter.Reset();

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
        } else {
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

    shared_win32_handle offscreen_renderer_dx::resize_frame(const vec2_i32 new_frame_size)
    {
        TRIENGINE_ASSERT(_flag_initialized);
        TRIENGINE_ASSERT(new_frame_size.x() > 0 && new_frame_size.y() > 0);

        ::glfwSetWindowSize(
            _glctx.get_glfw_window(),
            new_frame_size.x(),
            new_frame_size.y()
        );

        return _dx11_interop_color_tex_handle;
    }

    bool offscreen_renderer_dx::render(const uint64_t mutex_key)
    {
        TRIENGINE_ASSERT(_flag_initialized);

        if (_curr_scn_it == _scn_list.end()) {
            TRIENGINE_PANIC("No scenes added");
        }

        // Calculate frame delta time
        const double curr_frame_time = ::glfwGetTime();
        _frame_time_delta = curr_frame_time - _last_frame_time;
        _last_frame_time = curr_frame_time;
        const float frame_delta_f32 = static_cast<float>(_frame_time_delta);

        scene& target_scn = *(_curr_scn_it->get());
        abstract_camera& target_scn_camera = *target_scn.get_camera();
        target_scn_camera.set_viewport(view_port{ 0, 0, _curr_frame_size.x(), _curr_frame_size.y() });

        // Process camera input
        target_scn_camera.update_animation(frame_delta_f32);

        // https://registry.khronos.org/OpenGL/extensions/EXT/EXT_win32_keyed_mutex.txt
        // Acquire KeyedMutex for the interop texture
        // TRUE is returned if the wait succeeded.
        // FALSE is returned if the acquire operation timed out or failed.
        // No error is generated if the operation failed because it timed out.
        constexpr uint32_t mutex_wait_timeout = UINT32_MAX; // Wait infinitely for the mutex to become available
        if (const GLboolean mutex_acquired = ::glAcquireKeyedMutexWin32EXT(
            _gl_interop_color_tex_mem_object, // GLuint64 handle
            mutex_key, // Key
            mutex_wait_timeout// GLuint64 timeout
        ); !mutex_acquired) {
            // Failed to acquire the mutex - check if it's a timeout or error
            const GLenum gl_error = ::glGetError();
            if (gl_error == GL_NO_ERROR) {
                // Timeout - this is normal, just skip this frame
                //LOG_TRACE("KeyedMutex acquire timeout - skipping frame");
                return true; // Continue rendering next frame
            } else {
                // Actual error occurred
                TRIENGINE_ERROR("Failed to acquire keyed mutex - GL error: 0x%X", gl_error);
                return false;
            }
        }

        // Render scene directly to the shared interop texture
        _scn_renderer.render(
            _gl_fbo,
            _curr_frame_size.x(),
            _curr_frame_size.y(),
            target_scn
        );

        // Release KeyedMutex for the interop texture
        // TRUE is returned if the release operation succeeded.
        // FALSE is returned if the release operation failed.
        if (const GLboolean mutex_released = ::glReleaseKeyedMutexWin32EXT(
            _gl_interop_color_tex_mem_object,
            mutex_key
        ); !mutex_released) {
            // Failed to release the mutex
            TRIENGINE_ERROR("Failed to release keyed mutex for OpenGL interop texture (key: %llu, last error: %u)"
                , mutex_key
                , ::GetLastError()
            );
            return false;
        }

        _glctx.swap_buffers();
        _glctx.poll_window_events();
        return true;
    }

    void offscreen_renderer_dx::_resize_frame(const vec2_i32 new_frame_size)
    {
        TRIENGINE_ASSERT(new_frame_size.x() > 0 && new_frame_size.y() > 0);

        if (_flag_initialized && _curr_frame_size == new_frame_size) {
            return; // No need to resize if the size is the same
        }

        _curr_frame_size = new_frame_size;

        // Recreate dx11 interop color texture
        {
            D3D11_TEXTURE2D_DESC dx11_interop_texture_desc = { 0, };
            dx11_interop_texture_desc.Width = static_cast<UINT>(new_frame_size.x());
            dx11_interop_texture_desc.Height = static_cast<UINT>(new_frame_size.y());
            dx11_interop_texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // Always RGBA format
            dx11_interop_texture_desc.MipLevels = 1;
            dx11_interop_texture_desc.ArraySize = 1;
            dx11_interop_texture_desc.SampleDesc.Count = 1;
            dx11_interop_texture_desc.SampleDesc.Quality = 0;
            dx11_interop_texture_desc.Usage = D3D11_USAGE_DEFAULT;
            dx11_interop_texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            dx11_interop_texture_desc.CPUAccessFlags = 0;
            dx11_interop_texture_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

            THROW_IF_FAILED(_dx11_device2->CreateTexture2D(
                &dx11_interop_texture_desc,
                nullptr,
                &_dx11_interop_color_tex
            ));
        }

        // Get the native handle of the new dx11 interop color texture
        {
            ComPtr<IDXGIResource1> dxgiResource1;
            ASSERT_HR(_dx11_interop_color_tex.As(&dxgiResource1));
            HANDLE new_shared_handle{};
            THROW_IF_FAILED(dxgiResource1->CreateSharedHandle(
                nullptr, // SECURITY_ATTRIBUTES
                DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
                nullptr, // Name; Name is optional, but if specified, it should be unique across processes, can be accessed via `OpenSharedResourceByName`
                &new_shared_handle
            ));
            _dx11_interop_color_tex_handle.reset(
                new_shared_handle,
                ::CloseHandle
            );

            TRIENGINE_ASSERT(_dx11_interop_color_tex_handle != nullptr);
        }

        // Recreate OpenGL interop memory object
        // and import the D3D11 texture into OpenGL memory object
        if (_gl_interop_color_tex_mem_object) {
            // Wait for all OpenGL commands to complete before deleting the memory object
            ::glFinish();
            ::glDeleteMemoryObjectsEXT(1, &_gl_interop_color_tex_mem_object);
        }
        ::glCreateMemoryObjectsEXT(1, &_gl_interop_color_tex_mem_object);
        ::glImportMemoryWin32HandleEXT( // Reimport
            _gl_interop_color_tex_mem_object,
            0, // texture memory size; Pass 0 to let OpenGL query it from D3D11 resource
            GL_HANDLE_TYPE_D3D11_IMAGE_EXT,
            _dx11_interop_color_tex_handle.get()
        );

        // Check for reimport errors
        const GLenum import_error = ::glGetError();
        if (import_error != GL_NO_ERROR) {
            TRIENGINE_PANIC("Failed to import D3D11 texture into OpenGL memory object (GL error: 0x%X)", import_error);
        }

        // Recreate gl interop color texture using the imported memory
        if (_gl_interop_color_tex) {
            ::glDeleteTextures(1, &_gl_interop_color_tex);
        }
        ::glCreateTextures(GL_TEXTURE_2D, 1, &_gl_interop_color_tex);
        ::glTextureStorageMem2DEXT(
            _gl_interop_color_tex, // GLuint texture
            1, // GLsizei levels
            GL_RGBA8, // GLenum internalformat - Use RGBA8 as internal format (matching DirectX side)
            static_cast<GLsizei>(new_frame_size.x()), // GLsizei width
            static_cast<GLsizei>(new_frame_size.y()), // GLsizei height
            _gl_interop_color_tex_mem_object, // GLuint memory
            0 // GLuint64 offset
        );

        // Set texture parameters for interop texture
        ::glTextureParameteri(_gl_interop_color_tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        ::glTextureParameteri(_gl_interop_color_tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        ::glTextureParameteri(_gl_interop_color_tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        ::glTextureParameteri(_gl_interop_color_tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        //::glTextureParameteri(_gl_interop_color_tex, GL_TEXTURE_TILING_EXT, GL_OPTIMAL_TILING_EXT); // D3D11 side is D3D11_TEXTURE_LAYOUT_UNDEFINED

        // Attach the shared interop texture directly to the FBO for rendering
        ::glNamedFramebufferTexture(
            _gl_fbo, 
            GL_COLOR_ATTACHMENT0, 
            _gl_interop_color_tex, 
            0
        );

        // Check FBO completeness
        if (const auto status = ::glCheckNamedFramebufferStatus(_gl_fbo, GL_FRAMEBUFFER);
            status != GL_FRAMEBUFFER_COMPLETE) {
            TRIENGINE_PANIC("Framebuffer is not complete (status: 0x%X)", static_cast<uint32_t>(status));
        }

        // Resize viewport
        ::glViewport(0, 0, _curr_frame_size.x(), _curr_frame_size.y());
    }

} // namespace
