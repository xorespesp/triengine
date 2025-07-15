#pragma once
#include "common.hh"
#include "ipc_proto.hh"
#include "ipc_service.hh"

class viewer_process
{
public:
    viewer_process();
    ~viewer_process();

    void run();

private:
    void _connect_to_renderer_process();
    void _init_d3d_window();
    void _init_d3d_resources();

    LRESULT _wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void _render_frame();

private:
    // Process/IPC
    std::shared_ptr<ipc_client> _ipc_cli;
    utils::unique_handle _renderer_process_handle;

    std::mutex _shared_info_mtx;
    std::condition_variable _shared_info_cv;
    std::optional<ipc_proto::packets::shared_render_context_t> _shared_info;

    // D3D Window
    utils::unique_hwnd _viewer_hwnd;

    // D3D Resources (DX11.2 API base)
    ComPtr<IDXGIAdapter> _dxgi_adapter;
    ComPtr<ID3D11Device2> _dx11_device2;
    ComPtr<ID3D11DeviceContext2> _dx11_device_context2;
    ComPtr<IDXGISwapChain1> _dx11_swapchain1;
    ComPtr<ID3D11RenderTargetView> _dx11_rtv;
    ComPtr<ID3D11Texture2D> _dx11_shared_texture;
    ComPtr<ID3D11Texture2D> _dx11_screen_texture;
    ComPtr<IDXGIKeyedMutex> _dxgi_keyed_mutex;

    // Shader Resources
    ComPtr<ID3D11VertexShader> _dx11_vertex_shader;
    ComPtr<ID3D11PixelShader> _dx11_pixel_shader;
    ComPtr<ID3D11ShaderResourceView> _dx11_srv;
    ComPtr<ID3D11SamplerState> _dx11_sampler_state;

    bool _initialized{ false };
};