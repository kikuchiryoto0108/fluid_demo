//==============================================================================
//  File   : render_target.cpp
//  Brief  : 
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/21
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "render_target.h"

namespace Engine {

    bool RenderTarget::Create(ID3D11Device* device, uint32_t width, uint32_t height,
        DXGI_FORMAT format, bool hasDepth) {
        m_width = width;
        m_height = height;

        // テクスチャ作成
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = format;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &m_pTexture);
        if (FAILED(hr)) return false;

        // RTV作成
        hr = device->CreateRenderTargetView(m_pTexture.Get(), nullptr, &m_pRTV);
        if (FAILED(hr)) return false;

        // SRV作成
        hr = device->CreateShaderResourceView(m_pTexture.Get(), nullptr, &m_pSRV);
        if (FAILED(hr)) return false;

        // 深度バッファ（必要な場合）
        if (hasDepth) {
            D3D11_TEXTURE2D_DESC depthDesc = {};
            depthDesc.Width = width;
            depthDesc.Height = height;
            depthDesc.MipLevels = 1;
            depthDesc.ArraySize = 1;
            depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            depthDesc.SampleDesc.Count = 1;
            depthDesc.Usage = D3D11_USAGE_DEFAULT;
            depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

            hr = device->CreateTexture2D(&depthDesc, nullptr, &m_pDepthTexture);
            if (FAILED(hr)) return false;

            hr = device->CreateDepthStencilView(m_pDepthTexture.Get(), nullptr, &m_pDSV);
            if (FAILED(hr)) return false;
        }

        return true;
    }

    void RenderTarget::Release() {
        m_pSRV.Reset();
        m_pRTV.Reset();
        m_pDSV.Reset();
        m_pTexture.Reset();
        m_pDepthTexture.Reset();
    }

    void RenderTarget::Clear(ID3D11DeviceContext* context, float r, float g, float b, float a) {
        float color[4] = { r, g, b, a };
        context->ClearRenderTargetView(m_pRTV.Get(), color);
        if (m_pDSV) {
            context->ClearDepthStencilView(m_pDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
        }
    }

    void RenderTarget::SetAsTarget(ID3D11DeviceContext* context) {
        ID3D11RenderTargetView* rtv = m_pRTV.Get();
        context->OMSetRenderTargets(1, &rtv, m_pDSV.Get());

        D3D11_VIEWPORT vp = {};
        vp.Width = static_cast<float>(m_width);
        vp.Height = static_cast<float>(m_height);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        context->RSSetViewports(1, &vp);
    }
}