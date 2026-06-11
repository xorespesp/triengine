#pragma once
#define INITGUID
#include <windows.h>
#include <wrl/client.h> // Microsoft::WRL::ComPtr
#include <dxgi1_2.h> // DXGI 1.2 API header
#include <d3d11_2.h> // DX11.2 API header
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <xutl/debug/logger.hh>
#include <xutl/debug/assert.hh>

using namespace std::string_literals;
using namespace std::string_view_literals;
using namespace std::chrono_literals;
using Microsoft::WRL::ComPtr;

namespace Config
{
    constexpr const char* RENDERER_PROCESS_INST_NAME = "Global\\TriengineInterprocRendererServer";
    constexpr const char* RENDERER_SERVER_NAME = "triengine-interproc-renderer-srv";
}

#define ASSERT_HR(EXPR) ::utils::assert_hr_impl(EXPR, _XUTL_CURRENT_SOURCE_LOC())

namespace utils
{
    static inline void assert_hr_impl(
        const HRESULT hr,
        const _XUTL debug::source_loc& loc)
    {
        if (FAILED(hr)) {
            const std::string_view src_file_name{ loc.filename() };
            throw std::runtime_error{ fmt::format("[ERROR] HRESULT failed with code {:X} at {}:{}"
                , static_cast<uint64_t>(hr)
                , static_cast<int>(src_file_name.size())
                , src_file_name.data()
                , loc.line)
            };
        }
    }

    struct handle_deleter {
        void operator()(HANDLE handle) const {
            if (handle) { ::CloseHandle(handle); }
        }
    };
    using unique_handle = std::unique_ptr<std::remove_pointer_t<HANDLE>, handle_deleter>;
    using shared_handle = std::shared_ptr<std::remove_pointer_t<HANDLE>>;

    struct hwnd_deleter {
        void operator()(HWND hwnd) const {
            if (hwnd) { ::DestroyWindow(hwnd); }
        }
    };
    using unique_hwnd = std::unique_ptr<std::remove_pointer_t<HWND>, hwnd_deleter>;
    using shared_hwnd = std::shared_ptr<std::remove_pointer_t<HWND>>;

} // namespace