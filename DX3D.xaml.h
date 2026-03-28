#pragma once

#include "DX3D.g.h"
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <microsoft.ui.xaml.media.dxinterop.h>

namespace winrt::WinUINotes::implementation
{
    struct DX3D : DX3DT<DX3D>
    {
        DX3D();
        void InitializeDirectX();
        void InitializeShaders();
        void InitializeGeometry();
        void SetupRenderTarget();
        void Render();
        int32_t MyProperty();
        void MyProperty(int32_t value);

    private:
        winrt::com_ptr<ID3D11Device>           m_d3dDevice;
        winrt::com_ptr<ID3D11DeviceContext>    m_d3dContext;
        winrt::com_ptr<IDXGISwapChain1>        m_swapChain;
        winrt::com_ptr<ID3D11RenderTargetView> m_renderTargetView;
        winrt::com_ptr<ID3D11VertexShader>     m_vertexShader;
        winrt::com_ptr<ID3D11PixelShader>      m_pixelShader;
        winrt::com_ptr<ID3D11InputLayout>      m_inputLayout;
        winrt::com_ptr<ID3D11Buffer>           m_vertexBuffer;
        winrt::event_token                     m_loadedToken;
        UINT                                   m_vertexCount = 0;
    };
}

namespace winrt::WinUINotes::factory_implementation
{
    struct DX3D : DX3DT<DX3D, implementation::DX3D>
    {
    };
}