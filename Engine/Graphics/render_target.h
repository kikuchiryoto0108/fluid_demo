//==============================================================================
//  File   : render_target.h
//  Brief  : オフスクリーンレンダーターゲット
//==============================================================================
#pragma once

#include <d3d11.h>
#include <wrl/client.h>

namespace Engine {
    using Microsoft::WRL::ComPtr;

    class RenderTarget {
    public:
        RenderTarget() = default;
        ~RenderTarget() { Release(); }

        bool Create(ID3D11Device* device, uint32_t width, uint32_t height,
            DXGI_FORMAT format = DXGI_FORMAT_R32_FLOAT, bool hasDepth = false);
        void Release();

        void Clear(ID3D11DeviceContext* context, float r = 0, float g = 0, float b = 0, float a = 0);
        void SetAsTarget(ID3D11DeviceContext* context);

        ID3D11ShaderResourceView* GetSRV() const { return m_pSRV.Get(); }
        ID3D11RenderTargetView* GetRTV() const { return m_pRTV.Get(); }
        ID3D11DepthStencilView* GetDSV() const { return m_pDSV.Get(); }

        uint32_t GetWidth() const { return m_width; }
        uint32_t GetHeight() const { return m_height; }

    private:
        ComPtr<ID3D11Texture2D> m_pTexture;
        ComPtr<ID3D11RenderTargetView> m_pRTV;
        ComPtr<ID3D11ShaderResourceView> m_pSRV;
        ComPtr<ID3D11DepthStencilView> m_pDSV;
        ComPtr<ID3D11Texture2D> m_pDepthTexture;

        uint32_t m_width = 0;
        uint32_t m_height = 0;
    };
}
