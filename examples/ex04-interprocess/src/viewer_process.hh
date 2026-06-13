#pragma once
#include <triengine_interop/surface/proto/surface_proto.hh>
#include <triengine_interop/surface/surface_consumer.hh>

#include "common.hh"

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
    utils::unique_handle _renderer_process_handle; // spawned renderer process handle (owned for lifetime)

    // Shared-surface consumer (owns the IPC connection + the D3D device/surface interop)
    triengine_interop::surface::surface_consumer _consumer;

    // Present target owned by the viewer (created on _consumer.get_dx11_device())
    utils::unique_hwnd _viewer_hwnd; // D3D window handle
    ComPtr<IDXGISwapChain1> _dx11_swapchain1;
    ComPtr<ID3D11RenderTargetView> _dx11_rtv; // swap-chain back buffer RTV
    D3D11_VIEWPORT _viewport{};
};
