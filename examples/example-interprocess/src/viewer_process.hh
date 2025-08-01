#pragma once
#include "common.hh"
#include "ipc_proto.hh"
#include "ipc_service.hh"

class viewer_process
{
public:
    viewer_process(
        int32_t frame_width = 640, 
        int32_t frame_height = 480, 
        DXGI_FORMAT frame_format = DXGI_FORMAT_R8G8B8A8_UNORM
    );
    ~viewer_process();

    void run();

private:
    void _initialize(int32_t frame_width, int32_t frame_height, DXGI_FORMAT frame_format);
    LRESULT _wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void _render_frame();

private:
    // Process/IPC
    std::shared_ptr<ipc_client> _ipc_cli;
    utils::unique_handle _renderer_process_handle;

    // D3D Window
    utils::unique_hwnd _viewer_hwnd;

    // D3D Resources (DX11.2)
    ComPtr<IDXGIAdapter> _dxgi_adapter;
    ComPtr<ID3D11Device2> _dx11_device2;
    ComPtr<ID3D11DeviceContext2> _dx11_device_context2;
    ComPtr<ID3D11Texture2D> _dx11_shared_texture;
    ComPtr<ID3D11Texture2D> _dx11_screen_texture;
    ComPtr<IDXGIKeyedMutex> _dxgi_keyed_mutex;

    // D3D Pipeline Resources
    ComPtr<IDXGISwapChain1> _dx11_swapchain1;
    ComPtr<ID3D11VertexShader> _dx11_vertex_shader;
    ComPtr<ID3D11PixelShader> _dx11_pixel_shader;
    ComPtr<ID3D11RenderTargetView> _dx11_rtv;
    ComPtr<ID3D11ShaderResourceView> _dx11_srv;
    ComPtr<ID3D11SamplerState> _dx11_sampler_state;

    bool _initialized{ false };
};