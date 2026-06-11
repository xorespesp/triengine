#pragma once
#include "common.hh"
#include "ipc_proto.hh"
#include "ipc_service.hh"

class viewer_process
{
public:
    viewer_process(
        SIZE initial_frame_size = { 640, 480 },
        DXGI_FORMAT target_frame_format = DXGI_FORMAT_R8G8B8A8_UNORM
    );
    ~viewer_process();

    void run();

private:
    void _initialize(SIZE initial_frame_size, DXGI_FORMAT target_frame_format);
    void _resize_frame(SIZE new_frame_size);
    void _render_frame();
    LRESULT _wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    bool _fl_initialized{ false };
    bool _fl_render_interop_texture{ true }; // Whether to render using the interop texture or not

    // Process/IPC
    std::shared_ptr<ipc_client> _ipc_cli;
    utils::unique_handle _renderer_process_handle;

    // D3D Resources (DX11.2)
    utils::unique_hwnd _viewer_hwnd; // D3D window handle
    ComPtr<IDXGIAdapter> _dxgi_adapter;
    ComPtr<ID3D11Device2> _dx11_device2;
    ComPtr<ID3D11DeviceContext2> _dx11_device_context2;
    ComPtr<ID3D11Texture2D> _dx11_shared_texture;
    ComPtr<ID3D11Texture2D> _dx11_shared_texture_copy; // copy of the shared texture (non-shared)
    ComPtr<IDXGIKeyedMutex> _dxgi_keyed_mutex;

    // D3D Pipeline Resources
    ComPtr<IDXGISwapChain1> _dx11_swapchain1;
    ComPtr<ID3D11VertexShader> _dx11_vertex_shader;
    ComPtr<ID3D11PixelShader> _dx11_pixel_shader;
    ComPtr<ID3D11RenderTargetView> _dx11_rtv;
    ComPtr<ID3D11ShaderResourceView> _dx11_srv;
    ComPtr<ID3D11SamplerState> _dx11_sampler_state;
    D3D11_VIEWPORT _viewport{};
};