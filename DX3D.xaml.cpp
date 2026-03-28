#include "pch.h"
#include "DX3D.xaml.h"
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <microsoft.ui.xaml.media.dxinterop.h>
#if __has_include("DX3D.g.cpp")
#include "DX3D.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::WinUINotes::implementation
{
    DX3D::DX3D()
    {
        InitializeComponent();

        // Wait until the panel is laid out before touching DirectX
        m_loadedToken = DxSwapChainPanel().Loaded(
            [this](auto&&, auto&&)
            {
                try
                {
                    InitializeDirectX();
                    InitializeShaders();
                    InitializeGeometry();
                    SetupRenderTarget();
                    Render();
                }
                catch (winrt::hresult_error const& ex)
                {
                    auto hr = ex.code();
                    auto msg = ex.message();
                    OutputDebugStringW((L"HRESULT: "
                        + std::to_wstring((uint32_t)(int32_t)hr)
                        + L" | " + std::wstring(msg) + L"\n").c_str());
                    __debugbreak();
                }
            });
    }

    void DX3D::InitializeDirectX()
    {
        UINT creationFlags = 0;
#if defined(_DEBUG)
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL featureLevel;
        HRESULT hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            creationFlags, nullptr, 0, D3D11_SDK_VERSION,
            m_d3dDevice.put(), &featureLevel, m_d3dContext.put()
        );
        if (FAILED(hr)) throw hresult_error(hr);

        // Read actual panel size — must be non-zero for composition swap chains
        auto panel = DxSwapChainPanel();
        UINT width = static_cast<UINT>(panel.ActualWidth());
        UINT height = static_cast<UINT>(panel.ActualHeight());

        // Safety fallback — should not happen after Loaded fires, but just in case
        if (width == 0) width = 800;
        if (height == 0) height = 600;

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = width;    // <-- explicit, NOT 0
        desc.Height = height;   // <-- explicit, NOT 0
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.SampleDesc.Count = 1;

        com_ptr<IDXGIDevice1>  dxgiDevice;
        com_ptr<IDXGIAdapter>  adapter;
        com_ptr<IDXGIFactory2> factory;

        m_d3dDevice.as(dxgiDevice);
        dxgiDevice->GetAdapter(adapter.put());
        adapter->GetParent(IID_PPV_ARGS(factory.put()));

        hr = factory->CreateSwapChainForComposition(
            m_d3dDevice.get(), &desc, nullptr, m_swapChain.put()
        );
        if (FAILED(hr)) throw hresult_error(hr);

        com_ptr<ISwapChainPanelNative> panelNative;
        panel.as(panelNative);
        panelNative->SetSwapChain(m_swapChain.get());
    }

    void DX3D::InitializeShaders()
    {
        com_ptr<ID3DBlob> vsBlob, psBlob, errBlob;

        D3DCompileFromFile(L"VertexShader.hlsl", nullptr, nullptr,
            "main", "vs_5_0", 0, 0, vsBlob.put(), errBlob.put());

        D3DCompileFromFile(L"PixelShader.hlsl", nullptr, nullptr,
            "main", "ps_5_0", 0, 0, psBlob.put(), errBlob.put());

        m_d3dDevice->CreateVertexShader(
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
            nullptr, m_vertexShader.put());

        m_d3dDevice->CreatePixelShader(
            psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
            nullptr, m_pixelShader.put());

        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
              D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        m_d3dDevice->CreateInputLayout(
            layout, 1,
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
            m_inputLayout.put());
    }

    void DX3D::InitializeGeometry()
    {
        struct Vertex { float x, y; };

        // NDC coords: (0,0)=center, (-1,-1)=bottom-left, (1,1)=top-right
        Vertex points[] = {
            {  0.0f,  0.5f },   // top center
            {  0.5f, -0.5f },   // bottom right
            { -0.5f, -0.5f },   // bottom left
        };
        m_vertexCount = 3;

        D3D11_BUFFER_DESC bd{};
        bd.ByteWidth = sizeof(points);
        bd.Usage     = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = points;

        m_d3dDevice->CreateBuffer(&bd, &sd, m_vertexBuffer.put());
    }

    void DX3D::SetupRenderTarget()
    {
        // Get the back buffer texture from the swap chain
        com_ptr<ID3D11Texture2D> backBuffer;
        HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.put()));
        if (FAILED(hr)) throw hresult_error(hr);

        // Create a view so D3D11 knows how to write to it
        hr = m_d3dDevice->CreateRenderTargetView(
            backBuffer.get(),
            nullptr,
            m_renderTargetView.put()
        );
        if (FAILED(hr)) throw hresult_error(hr);

        // Tell the Output Merger stage: draw into THIS target
        ID3D11RenderTargetView* rtv = m_renderTargetView.get();
        m_d3dContext->OMSetRenderTargets(1, &rtv, nullptr);

        // Set the viewport so D3D11 knows what region of the screen to draw to
        auto panel = DxSwapChainPanel();
        D3D11_VIEWPORT vp{};
        vp.Width    = static_cast<float>(panel.ActualWidth());
        vp.Height   = static_cast<float>(panel.ActualHeight());
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        m_d3dContext->RSSetViewports(1, &vp);
    }

    void DX3D::Render()
    {
        // Clear the back buffer to dark blue
        float clearColor[4] = { 0.1f, 0.2f, 0.4f, 1.0f };
        m_d3dContext->ClearRenderTargetView(m_renderTargetView.get(), clearColor);

        // Bind shaders
        m_d3dContext->VSSetShader(m_vertexShader.get(), nullptr, 0);
        m_d3dContext->PSSetShader(m_pixelShader.get(), nullptr, 0);
        m_d3dContext->IASetInputLayout(m_inputLayout.get());

        // Bind vertex buffer
        ID3D11Buffer* vb = m_vertexBuffer.get();
        UINT stride = sizeof(float) * 2;
        UINT offset = 0;
        m_d3dContext->IASetVertexBuffers(0, 1, &vb, &stride, &offset);

        // Draw a filled triangle
        m_d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_d3dContext->Draw(m_vertexCount, 0);

        // Flip the back buffer to the screen (1 = wait for vsync)
        m_swapChain->Present(1, 0);
    }

    int32_t DX3D::MyProperty()
    {
        throw hresult_not_implemented();
    }

    void DX3D::MyProperty(int32_t value)
    {
        throw hresult_not_implemented();
    }
}