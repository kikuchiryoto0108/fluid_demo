//==============================================================================
//  File   : renderer.cpp
//  Brief  : レンダラー - シングルトン描画システム
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "renderer.h"
#include "Engine/Graphics/vertex.h"
#include <d3dcompiler.h>
#include <cstring>
#include <cmath>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace Engine {

    //==========================================================
    // シングルトンインスタンス取得
    //==========================================================
    Renderer& Renderer::GetInstance() {
        static Renderer instance;
        return instance;
    }

    //==========================================================
    // Initialize - D3D11デバイス・スワップチェーン・シェーダーの初期化
    //==========================================================
    bool Renderer::Initialize(HINSTANCE hInstance, HWND hWnd, bool windowed) {
        HRESULT hr = S_OK;

        // --- スワップチェーン設定 ---
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1;
        sd.BufferDesc.Width = m_screenWidth;
        sd.BufferDesc.Height = m_screenHeight;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hWnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = windowed ? TRUE : FALSE;

        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };
        D3D_FEATURE_LEVEL featureLevel;

        // --- デバイスとスワップチェーンを作成（まずハードウェアを試す） ---
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            featureLevels, ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION, &sd,
            &m_pSwapChain, &m_pDevice, &featureLevel, &m_pContext
        );

        // ハードウェアがダメならソフトウェア（WARP）にフォールバック
        if (FAILED(hr)) {
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                featureLevels, ARRAYSIZE(featureLevels),
                D3D11_SDK_VERSION, &sd,
                &m_pSwapChain, &m_pDevice, &featureLevel, &m_pContext
            );
            if (FAILED(hr)) return false;
        }

        // --- レンダーターゲットビューを作成 ---
        ComPtr<ID3D11Texture2D> pBackBuffer;
        m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &pBackBuffer);
        m_pDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &m_pRenderTargetView);

        // --- 深度ステンシルテクスチャとビューを作成 ---
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = m_screenWidth;
        td.Height = m_screenHeight;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        ComPtr<ID3D11Texture2D> pDepthTexture;
        m_pDevice->CreateTexture2D(&td, nullptr, &pDepthTexture);

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvd = {};
        dsvd.Format = td.Format;
        dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        m_pDevice->CreateDepthStencilView(pDepthTexture.Get(), &dsvd, &m_pDepthStencilView);

        // レンダーターゲットと深度バッファをバインド
        ID3D11RenderTargetView* rtv = m_pRenderTargetView.Get();
        m_pContext->OMSetRenderTargets(1, &rtv, m_pDepthStencilView.Get());

        // --- ビューポートを設定 ---
        D3D11_VIEWPORT vp = {};
        vp.Width = static_cast<float>(m_screenWidth);
        vp.Height = static_cast<float>(m_screenHeight);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        m_pContext->RSSetViewports(1, &vp);

        // --- ラスタライザーステートを設定 ---
        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_BACK;
        rd.DepthClipEnable = TRUE;

        ComPtr<ID3D11RasterizerState> pRasterizerState;
        m_pDevice->CreateRasterizerState(&rd, &pRasterizerState);
        m_pContext->RSSetState(pRasterizerState.Get());

        // --- アルファブレンドを有効にする ---
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        ComPtr<ID3D11BlendState> pBlendState;
        m_pDevice->CreateBlendState(&blendDesc, &pBlendState);
        float blendFactor[4] = { 0, 0, 0, 0 };
        m_pContext->OMSetBlendState(pBlendState.Get(), blendFactor, 0xffffffff);

        // --- 深度ステンシルステートを作成（有効/無効の2種類） ---
        D3D11_DEPTH_STENCIL_DESC dsDesc = {};
        dsDesc.DepthEnable = TRUE;
        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
        m_pDevice->CreateDepthStencilState(&dsDesc, &m_pDepthStateEnable);

        dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        m_pDevice->CreateDepthStencilState(&dsDesc, &m_pDepthStateDisable);

        m_pContext->OMSetDepthStencilState(m_pDepthStateEnable.Get(), 0);

        // --- テクスチャサンプラーを設定 ---
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.MaxAnisotropy = 16;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

        ComPtr<ID3D11SamplerState> pSamplerState;
        m_pDevice->CreateSamplerState(&samplerDesc, &pSamplerState);
        ID3D11SamplerState* sampler = pSamplerState.Get();
        m_pContext->PSSetSamplers(0, 1, &sampler);

        // --- 頂点シェーダーをコンパイル・作成 ---
        ComPtr<ID3DBlob> pVSBlob;
        ComPtr<ID3DBlob> pErrorBlob;

        hr = D3DCompileFromFile(L"shader.hlsl", nullptr, nullptr, "VertexShaderPolygon", "vs_4_0_level_9_3",
            D3DCOMPILE_ENABLE_STRICTNESS, 0, &pVSBlob, &pErrorBlob);
        if (FAILED(hr)) {
            if (pErrorBlob) {
                MessageBoxA(nullptr, (char*)pErrorBlob->GetBufferPointer(), "VS Error", MB_OK);
            }
            return false;
        }

        m_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_pVertexShader);

        // --- 入力レイアウトを作成 ---
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
            pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pInputLayout);

        // --- ピクセルシェーダーをコンパイル・作成 ---
        ComPtr<ID3DBlob> pPSBlob;
        pErrorBlob.Reset();
        hr = D3DCompileFromFile(L"shader.hlsl", nullptr, nullptr, "PixelShaderPolygon", "ps_4_0_level_9_3",
            D3DCOMPILE_ENABLE_STRICTNESS, 0, &pPSBlob, &pErrorBlob);
        if (FAILED(hr)) {
            if (pErrorBlob) {
                MessageBoxA(nullptr, (char*)pErrorBlob->GetBufferPointer(), "PS Error", MB_OK);
            }
            return false;
        }

        m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pPixelShader);

        // --- WVP定数バッファを作成 ---
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = sizeof(XMMATRIX);
        cbDesc.Usage = D3D11_USAGE_DEFAULT;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pConstantBuffer);
        ID3D11Buffer* cb = m_pConstantBuffer.Get();
        m_pContext->VSSetConstantBuffers(0, 1, &cb);

        // --- マテリアル定数バッファを作成 ---
        cbDesc.ByteWidth = 80;  // MaterialDataのサイズ
        m_pDevice->CreateBuffer(&cbDesc, nullptr, &m_pMaterialBuffer);
        ID3D11Buffer* mb = m_pMaterialBuffer.Get();
        m_pContext->VSSetConstantBuffers(1, 1, &mb);

        // マテリアルバッファを白で初期化
        struct {
            XMFLOAT4 diffuse;
            XMFLOAT4 ambient;
            XMFLOAT4 specular;
            XMFLOAT4 emission;
            float    shininess;
            float    padding[3];
        } initialMaterial = {
            {1.0f, 1.0f, 1.0f, 1.0f},  // diffuse
            {1.0f, 1.0f, 1.0f, 1.0f},  // ambient
            {0.0f, 0.0f, 0.0f, 1.0f},  // specular
            {0.0f, 0.0f, 0.0f, 1.0f},  // emission
            0.0f,                       // shininess
            {0.0f, 0.0f, 0.0f}         // padding
        };
        m_pContext->UpdateSubresource(m_pMaterialBuffer.Get(), 0, nullptr, &initialMaterial, 0, 0);

        // --- シェーダーと入力レイアウトをパイプラインにセット ---
        m_pContext->IASetInputLayout(m_pInputLayout.Get());
        m_pContext->VSSetShader(m_pVertexShader.Get(), nullptr, 0);
        m_pContext->PSSetShader(m_pPixelShader.Get(), nullptr, 0);

        // --- 2D UI描画用リソースの初期化 ---
        InitUI();

        return true;
    }

    //==========================================================
    // Finalize - 全リソースの解放
    //==========================================================
    void Renderer::Finalize() {
        FinalizeUI();

        if (m_pContext) {
            m_pContext->ClearState();
        }

        m_pConstantBuffer.Reset();
        m_pMaterialBuffer.Reset();
        m_pInputLayout.Reset();
        m_pVertexShader.Reset();
        m_pPixelShader.Reset();
        m_pDepthStateEnable.Reset();
        m_pDepthStateDisable.Reset();
        m_pDepthStencilView.Reset();
        m_pRenderTargetView.Reset();
        m_pSwapChain.Reset();
        m_pContext.Reset();
        m_pDevice.Reset();
    }

    //==========================================================
    // Clear - 画面と深度バッファをクリアする
    //==========================================================
    void Renderer::Clear(float r, float g, float b, float a) {
        float clearColor[4] = { r, g, b, a };
        m_pContext->ClearRenderTargetView(m_pRenderTargetView.Get(), clearColor);
        m_pContext->ClearDepthStencilView(m_pDepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    }

    //==========================================================
    // Present - バックバッファを画面に表示する
    //==========================================================
    void Renderer::Present() {
        m_pSwapChain->Present(0, 0);
    }

    //==========================================================
    // SetDepthEnable - 深度テストの有効/無効を切り替える
    //==========================================================
    void Renderer::SetDepthEnable(bool enable) {
        m_pContext->OMSetDepthStencilState(
            enable ? m_pDepthStateEnable.Get() : m_pDepthStateDisable.Get(), 0);
    }

    //==========================================================
    // 行列設定（設定後に定数バッファを即座に更新する）
    //==========================================================
    void Renderer::SetWorldMatrix(const XMMATRIX& matrix) {
        m_worldMatrix = matrix;
        UpdateConstantBuffer();
    }

    void Renderer::SetViewMatrix(const XMMATRIX& matrix) {
        m_viewMatrix = matrix;
        UpdateConstantBuffer();
    }

    void Renderer::SetProjectionMatrix(const XMMATRIX& matrix) {
        m_projectionMatrix = matrix;
        UpdateConstantBuffer();
    }

    //==========================================================
    // UpdateConstantBuffer - WVP行列を転置してGPUに転送する
    //==========================================================
    void Renderer::UpdateConstantBuffer() {
        XMMATRIX wvp = m_worldMatrix * m_viewMatrix * m_projectionMatrix;
        wvp = XMMatrixTranspose(wvp);

        XMFLOAT4X4 matrix;
        XMStoreFloat4x4(&matrix, wvp);
        m_pContext->UpdateSubresource(m_pConstantBuffer.Get(), 0, nullptr, &matrix, 0, 0);
    }

    //==========================================================
    // SetupFor2D - 2D描画モードに切り替える
    // 正射影行列（左上原点、ピクセル座標系）、深度テスト無効
    //==========================================================
    void Renderer::SetupFor2D() {
        m_projectionMatrix = XMMatrixOrthographicOffCenterLH(
            0.0f, static_cast<float>(m_screenWidth),
            static_cast<float>(m_screenHeight), 0.0f,
            0.0f, 1.0f
        );
        m_viewMatrix = XMMatrixIdentity();
        m_worldMatrix = XMMatrixIdentity();
        SetDepthEnable(false);
        UpdateConstantBuffer();
    }

    //==========================================================
    // SetupFor3D - 3D描画モードに戻す（深度テスト有効）
    //==========================================================
    void Renderer::SetupFor3D() {
        SetDepthEnable(true);
    }

    //==========================================================
    //
    //  以下、2D UI描画システムの実装
    //
    //==========================================================

    //----------------------------------------------------------
    // ビットマップフォントのドットデータ
    // ASCII 32(' ')〜126('~')の95文字分
    // 各文字は8x12ピクセル
    // 1行 = 8bit = 1バイト、12行 = 12バイト/文字
    // ビット配置: MSB(bit7) = 左端ピクセル
    //----------------------------------------------------------
    static const unsigned char g_fontData[][12] = {
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 32: ' '
        {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x18,0x00,0x00}, // 33: '!'
        {0x6C,0x6C,0x6C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 34: '"'
        {0x6C,0x6C,0xFE,0x6C,0x6C,0xFE,0x6C,0x6C,0x00,0x00,0x00,0x00}, // 35: '#'
        {0x18,0x7E,0xD8,0xD8,0x7E,0x1B,0x1B,0x7E,0x18,0x00,0x00,0x00}, // 36: '$'
        {0x00,0xC6,0xCC,0x18,0x30,0x60,0xC6,0x06,0x00,0x00,0x00,0x00}, // 37: '%'
        {0x38,0x6C,0x38,0x76,0xDC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00}, // 38: '&'
        {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 39: '''
        {0x0C,0x18,0x30,0x30,0x30,0x30,0x30,0x18,0x0C,0x00,0x00,0x00}, // 40: '('
        {0x30,0x18,0x0C,0x0C,0x0C,0x0C,0x0C,0x18,0x30,0x00,0x00,0x00}, // 41: ')'
        {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00,0x00,0x00,0x00,0x00}, // 42: '*'
        {0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,0x00,0x00,0x00}, // 43: '+'
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30,0x00,0x00}, // 44: ','
        {0x00,0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 45: '-'
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00}, // 46: '.'
        {0x06,0x0C,0x18,0x30,0x60,0xC0,0x00,0x00,0x00,0x00,0x00,0x00}, // 47: '/'
        {0x7C,0xC6,0xCE,0xDE,0xF6,0xE6,0xC6,0x7C,0x00,0x00,0x00,0x00}, // 48: '0'
        {0x18,0x38,0x78,0x18,0x18,0x18,0x18,0x7E,0x00,0x00,0x00,0x00}, // 49: '1'
        {0x7C,0xC6,0x06,0x0C,0x18,0x30,0x60,0xFE,0x00,0x00,0x00,0x00}, // 50: '2'
        {0x7C,0xC6,0x06,0x3C,0x06,0x06,0xC6,0x7C,0x00,0x00,0x00,0x00}, // 51: '3'
        {0x0C,0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x0C,0x00,0x00,0x00,0x00}, // 52: '4'
        {0xFE,0xC0,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00,0x00,0x00,0x00}, // 53: '5'
        {0x3C,0x60,0xC0,0xFC,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00}, // 54: '6'
        {0xFE,0x06,0x0C,0x18,0x30,0x30,0x30,0x30,0x00,0x00,0x00,0x00}, // 55: '7'
        {0x7C,0xC6,0xC6,0x7C,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00}, // 56: '8'
        {0x7C,0xC6,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00,0x00,0x00,0x00}, // 57: '9'
        {0x00,0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00}, // 58: ':'
        {0x00,0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30,0x00,0x00,0x00}, // 59: ';'
        {0x06,0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x06,0x00,0x00,0x00}, // 60: '<'
        {0x00,0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00}, // 61: '='
        {0x60,0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x60,0x00,0x00,0x00}, // 62: '>'
        {0x7C,0xC6,0x06,0x0C,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00}, // 63: '?'
        {0x7C,0xC6,0xDE,0xDE,0xDE,0xC0,0x7E,0x00,0x00,0x00,0x00,0x00}, // 64: '@'
        {0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00}, // 65: 'A'
        {0xFC,0xC6,0xC6,0xFC,0xC6,0xC6,0xC6,0xFC,0x00,0x00,0x00,0x00}, // 66: 'B'
        {0x7C,0xC6,0xC0,0xC0,0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00}, // 67: 'C'
        {0xF8,0xCC,0xC6,0xC6,0xC6,0xC6,0xCC,0xF8,0x00,0x00,0x00,0x00}, // 68: 'D'
        {0xFE,0xC0,0xC0,0xFC,0xC0,0xC0,0xC0,0xFE,0x00,0x00,0x00,0x00}, // 69: 'E'
        {0xFE,0xC0,0xC0,0xFC,0xC0,0xC0,0xC0,0xC0,0x00,0x00,0x00,0x00}, // 70: 'F'
        {0x7C,0xC6,0xC0,0xC0,0xCE,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00}, // 71: 'G'
        {0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00}, // 72: 'H'
        {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x00,0x00,0x00,0x00}, // 73: 'I'
        {0x1E,0x06,0x06,0x06,0x06,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00}, // 74: 'J'
        {0xC6,0xCC,0xD8,0xF0,0xD8,0xCC,0xC6,0xC6,0x00,0x00,0x00,0x00}, // 75: 'K'
        {0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xFE,0x00,0x00,0x00,0x00}, // 76: 'L'
        {0xC6,0xEE,0xFE,0xD6,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00}, // 77: 'M'
        {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00}, // 78: 'N'
        {0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00}, // 79: 'O'
        {0xFC,0xC6,0xC6,0xFC,0xC0,0xC0,0xC0,0xC0,0x00,0x00,0x00,0x00}, // 80: 'P'
        {0x7C,0xC6,0xC6,0xC6,0xC6,0xD6,0xDE,0x7C,0x06,0x00,0x00,0x00}, // 81: 'Q'
        {0xFC,0xC6,0xC6,0xFC,0xD8,0xCC,0xC6,0xC6,0x00,0x00,0x00,0x00}, // 82: 'R'
        {0x7C,0xC6,0xC0,0x7C,0x06,0x06,0xC6,0x7C,0x00,0x00,0x00,0x00}, // 83: 'S'
        {0xFE,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00}, // 84: 'T'
        {0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00}, // 85: 'U'
        {0xC6,0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00,0x00,0x00,0x00}, // 86: 'V'
        {0xC6,0xC6,0xC6,0xC6,0xD6,0xFE,0xEE,0xC6,0x00,0x00,0x00,0x00}, // 87: 'W'
        {0xC6,0xC6,0x6C,0x38,0x38,0x6C,0xC6,0xC6,0x00,0x00,0x00,0x00}, // 88: 'X'
        {0xCC,0xCC,0xCC,0x78,0x30,0x30,0x30,0x30,0x00,0x00,0x00,0x00}, // 89: 'Y'
        {0xFE,0x06,0x0C,0x18,0x30,0x60,0xC0,0xFE,0x00,0x00,0x00,0x00}, // 90: 'Z'
        {0x3C,0x30,0x30,0x30,0x30,0x30,0x30,0x3C,0x00,0x00,0x00,0x00}, // 91: '['
        {0xC0,0x60,0x30,0x18,0x0C,0x06,0x00,0x00,0x00,0x00,0x00,0x00}, // 92: '\'
        {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00,0x00,0x00,0x00}, // 93: ']'
        {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 94: '^'
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0x00,0x00,0x00}, // 95: '_'
        {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 96: '`'
        {0x00,0x00,0x7C,0x06,0x7E,0xC6,0xC6,0x7E,0x00,0x00,0x00,0x00}, // 97: 'a'
        {0xC0,0xC0,0xFC,0xC6,0xC6,0xC6,0xC6,0xFC,0x00,0x00,0x00,0x00}, // 98: 'b'
        {0x00,0x00,0x7C,0xC6,0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00}, // 99: 'c'
        {0x06,0x06,0x7E,0xC6,0xC6,0xC6,0xC6,0x7E,0x00,0x00,0x00,0x00}, // 100: 'd'
        {0x00,0x00,0x7C,0xC6,0xFE,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00}, // 101: 'e'
        {0x1C,0x36,0x30,0x7C,0x30,0x30,0x30,0x30,0x00,0x00,0x00,0x00}, // 102: 'f'
        {0x00,0x00,0x7E,0xC6,0xC6,0xC6,0x7E,0x06,0x7C,0x00,0x00,0x00}, // 103: 'g'
        {0xC0,0xC0,0xFC,0xC6,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00}, // 104: 'h'
        {0x18,0x00,0x38,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00}, // 105: 'i'
        {0x0C,0x00,0x1C,0x0C,0x0C,0x0C,0x0C,0xCC,0x78,0x00,0x00,0x00}, // 106: 'j'
        {0xC0,0xC0,0xCC,0xD8,0xF0,0xD8,0xCC,0xC6,0x00,0x00,0x00,0x00}, // 107: 'k'
        {0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00}, // 108: 'l'
        {0x00,0x00,0xCC,0xFE,0xD6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00}, // 109: 'm'
        {0x00,0x00,0xFC,0xC6,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00}, // 110: 'n'
        {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00}, // 111: 'o'
        {0x00,0x00,0xFC,0xC6,0xC6,0xC6,0xFC,0xC0,0xC0,0x00,0x00,0x00}, // 112: 'p'
        {0x00,0x00,0x7E,0xC6,0xC6,0xC6,0x7E,0x06,0x06,0x00,0x00,0x00}, // 113: 'q'
        {0x00,0x00,0xDC,0xE6,0xC0,0xC0,0xC0,0xC0,0x00,0x00,0x00,0x00}, // 114: 'r'
        {0x00,0x00,0x7E,0xC0,0x7C,0x06,0x06,0xFC,0x00,0x00,0x00,0x00}, // 115: 's'
        {0x30,0x30,0x7C,0x30,0x30,0x30,0x36,0x1C,0x00,0x00,0x00,0x00}, // 116: 't'
        {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0x7E,0x00,0x00,0x00,0x00}, // 117: 'u'
        {0x00,0x00,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00,0x00,0x00,0x00}, // 118: 'v'
        {0x00,0x00,0xC6,0xC6,0xD6,0xFE,0xEE,0xC6,0x00,0x00,0x00,0x00}, // 119: 'w'
        {0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00,0x00}, // 120: 'x'
        {0x00,0x00,0xC6,0xC6,0xC6,0x7E,0x06,0x7C,0x00,0x00,0x00,0x00}, // 121: 'y'
        {0x00,0x00,0xFE,0x0C,0x18,0x30,0x60,0xFE,0x00,0x00,0x00,0x00}, // 122: 'z'
        {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00,0x00,0x00,0x00,0x00}, // 123: '{'
        {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00}, // 124: '|'
        {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00,0x00,0x00,0x00,0x00}, // 125: '}'
        {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 126: '~'
    };

    // フォントの文字数（ASCII 32〜126 = 95文字）
    static const int FONT_GLYPH_COUNT = sizeof(g_fontData) / sizeof(g_fontData[0]);

    //==========================================================
    // InitUI - 2D描画用のGPUリソースを一括作成する
    //==========================================================
    bool Renderer::InitUI() {
        if (m_uiInitialized) return true;

        // --- 矩形描画用の動的頂点バッファ（6頂点 = 三角形2枚） ---
        {
            D3D11_BUFFER_DESC bd = {};
            bd.Usage = D3D11_USAGE_DYNAMIC;
            bd.ByteWidth = sizeof(Vertex3D) * 6;
            bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            if (FAILED(m_pDevice->CreateBuffer(&bd, nullptr, &m_pUIVertexBuffer))) {
                return false;
            }
        }

        // --- テキスト描画用の動的頂点バッファ（1文字=6頂点 × 最大128文字） ---
        {
            D3D11_BUFFER_DESC bd = {};
            bd.Usage = D3D11_USAGE_DYNAMIC;
            bd.ByteWidth = sizeof(Vertex3D) * 6 * MAX_TEXT_CHARS;
            bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            if (FAILED(m_pDevice->CreateBuffer(&bd, nullptr, &m_pTextVertexBuffer))) {
                return false;
            }
        }

        // --- ビットマップフォントテクスチャの生成 ---
        if (!CreateFontTexture()) {
            return false;
        }

        // --- 1x1白テクスチャの生成 ---
        if (!CreateWhiteTexture()) {
            return false;
        }

        m_uiInitialized = true;
        return true;
    }

    //==========================================================
    // FinalizeUI - UI用リソースを全て解放する
    //==========================================================
    void Renderer::FinalizeUI() {
        m_pUIVertexBuffer.Reset();
        m_pTextVertexBuffer.Reset();
        m_pFontSRV.Reset();
        m_pWhiteSRV.Reset();
        m_uiInitialized = false;
    }

    //==========================================================
    // CreateFontTexture - ドットデータからフォントテクスチャを生成する
    // g_fontDataの各ビットを白色不透明ピクセルに変換し、
    // 128x72のテクスチャとしてGPUに転送する（16文字x6行のグリッド）
    //==========================================================
    bool Renderer::CreateFontTexture() {
        const int texW = FONT_TEX_W;  // 128px
        const int texH = FONT_TEX_H;  // 72px

        // 全て透明で初期化
        std::vector<uint32_t> pixels(texW * texH, 0x00000000);

        // 各文字のドットデータをテクスチャに書き込む
        for (int glyphIdx = 0; glyphIdx < FONT_GLYPH_COUNT; glyphIdx++) {
            int col = glyphIdx % FONT_COLS;
            int row = glyphIdx / FONT_COLS;
            int baseX = col * FONT_CHAR_W;
            int baseY = row * FONT_CHAR_H;

            for (int y = 0; y < FONT_CHAR_H; y++) {
                unsigned char line = g_fontData[glyphIdx][y];
                for (int x = 0; x < FONT_CHAR_W; x++) {
                    // MSBから順にチェック（bit7 = 左端ピクセル）
                    if (line & (0x80 >> x)) {
                        int px = baseX + x;
                        int py = baseY + y;
                        pixels[py * texW + px] = 0xFFFFFFFF;  // 白色不透明
                    }
                }
            }
        }

        // GPU用テクスチャを作成
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = texW;
        td.Height = texH;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = pixels.data();
        initData.SysMemPitch = texW * sizeof(uint32_t);

        ComPtr<ID3D11Texture2D> tex;
        if (FAILED(m_pDevice->CreateTexture2D(&td, &initData, &tex))) {
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = td.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        if (FAILED(m_pDevice->CreateShaderResourceView(tex.Get(), &srvDesc, &m_pFontSRV))) {
            return false;
        }

        return true;
    }

    //==========================================================
    // CreateWhiteTexture - 1x1白テクスチャを生成する
    // 矩形描画時にテクスチャ乗算を無効化するため
    //==========================================================
    bool Renderer::CreateWhiteTexture() {
        uint32_t whitePixel = 0xFFFFFFFF;

        D3D11_TEXTURE2D_DESC td = {};
        td.Width = 1;
        td.Height = 1;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = &whitePixel;
        initData.SysMemPitch = sizeof(uint32_t);

        ComPtr<ID3D11Texture2D> tex;
        if (FAILED(m_pDevice->CreateTexture2D(&td, &initData, &tex))) {
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = td.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        return SUCCEEDED(m_pDevice->CreateShaderResourceView(tex.Get(), &srvDesc, &m_pWhiteSRV));
    }

    //==========================================================
    // DrawRect2D - 画面上に色付き矩形を描画する
    //
    // 処理の流れ:
    //   1. 3D行列を保存 → 2Dモードに切替
    //   2. 白テクスチャをバインド（テクスチャ×頂点カラー = 頂点カラーそのまま）
    //   3. 三角形2枚で四角形を描画
    //   4. 3Dモードに復帰
    //==========================================================
    void Renderer::DrawRect2D(float x, float y, float width, float height, const XMFLOAT4& col) {
        if (!m_uiInitialized) return;

        // 現在の行列を保存
        XMMATRIX savedWorld = m_worldMatrix;
        XMMATRIX savedView = m_viewMatrix;
        XMMATRIX savedProj = m_projectionMatrix;

        SetupFor2D();

        // 四角形の頂点座標
        float x0 = x;
        float y0 = y;
        float x1 = x + width;
        float y1 = y + height;

        // 頂点を作るヘルパー（Vertex3Dのメンバ名: position, normal, color, texCoord）
        auto makeVert = [&](float vx, float vy) -> Vertex3D {
            Vertex3D v;
            v.position = { vx, vy, 0.0f };
            v.normal = { 0.0f, 0.0f, -1.0f };
            v.color = col;
            v.texCoord = { 0.0f, 0.0f };
            return v;
            };

        // 6頂点で三角形2枚
        Vertex3D verts[6];
        verts[0] = makeVert(x0, y0);  // 左上
        verts[1] = makeVert(x1, y0);  // 右上
        verts[2] = makeVert(x0, y1);  // 左下
        verts[3] = makeVert(x1, y0);  // 右上
        verts[4] = makeVert(x1, y1);  // 右下
        verts[5] = makeVert(x0, y1);  // 左下

        // 動的バッファに書き込む
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(m_pContext->Map(m_pUIVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, verts, sizeof(verts));
            m_pContext->Unmap(m_pUIVertexBuffer.Get(), 0);
        }

        // マテリアルを白に設定
        struct MaterialData {
            XMFLOAT4 diffuse;
            XMFLOAT4 ambient;
            XMFLOAT4 specular;
            XMFLOAT4 emission;
            float    shininess;
            float    padding[3];
        };
        MaterialData mat = {};
        mat.diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
        m_pContext->UpdateSubresource(m_pMaterialBuffer.Get(), 0, nullptr, &mat, 0, 0);

        // 白テクスチャをバインド
        ID3D11ShaderResourceView* whiteSRV = m_pWhiteSRV.Get();
        m_pContext->PSSetShaderResources(0, 1, &whiteSRV);

        // 描画
        UINT stride = sizeof(Vertex3D);
        UINT offset = 0;
        ID3D11Buffer* vb = m_pUIVertexBuffer.Get();
        m_pContext->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pContext->Draw(6, 0);

        // 3Dモードに復帰
        SetupFor3D();
        m_worldMatrix = savedWorld;
        m_viewMatrix = savedView;
        m_projectionMatrix = savedProj;
        UpdateConstantBuffer();
    }

    //==========================================================
    // DrawText2D - 画面上にASCIIテキストを描画する
    //
    // 処理の流れ:
    //   1. 3D行列を保存 → 2Dモードに切替
    //   2. 各文字をフォントテクスチャ上のUVにマッピング
    //   3. 1文字 = 6頂点（三角形2枚）で頂点データを構築
    //   4. フォントテクスチャをバインドして一括描画
    //   5. 3Dモードに復帰
    //
    // 表示サイズ: ドットフォントの2倍拡大（8x12 → 16x24ピクセル）
    //==========================================================
    void Renderer::DrawText2D(float x, float y, const char* text, const XMFLOAT4& col) {
        if (!m_uiInitialized || !text || !m_pFontSRV) return;

        // 現在の行列を保存
        XMMATRIX savedWorld = m_worldMatrix;
        XMMATRIX savedView = m_viewMatrix;
        XMMATRIX savedProj = m_projectionMatrix;

        SetupFor2D();

        // 文字数を取得（上限チェック）
        int len = (int)strlen(text);
        if (len > MAX_TEXT_CHARS) len = MAX_TEXT_CHARS;
        if (len == 0) {
            SetupFor3D();
            m_worldMatrix = savedWorld;
            m_viewMatrix = savedView;
            m_projectionMatrix = savedProj;
            UpdateConstantBuffer();
            return;
        }

        // フォントテクスチャのサイズ（UV正規化用）
        float texW = (float)FONT_TEX_W;
        float texH = (float)FONT_TEX_H;

        // 1文字の表示サイズ（2倍拡大）
        float charDisplayW = (float)FONT_CHAR_W * 2.0f;  // 16px
        float charDisplayH = (float)FONT_CHAR_H * 2.0f;  // 24px

        // 全文字分の頂点データを構築
        std::vector<Vertex3D> verts;
        verts.reserve(len * 6);

        float cursorX = x;
        for (int i = 0; i < len; i++) {
            unsigned char ch = (unsigned char)text[i];

            // ASCII 32〜126の範囲外はスペース扱い（カーソルだけ進める）
            if (ch < 32 || ch > 126) {
                cursorX += charDisplayW;
                continue;
            }

            // フォントテクスチャ上のグリフ位置を計算
            int glyphIdx = ch - 32;
            int gc = glyphIdx % FONT_COLS;
            int gr = glyphIdx / FONT_COLS;

            // UV座標
            float u0 = (float)(gc * FONT_CHAR_W) / texW;
            float v0 = (float)(gr * FONT_CHAR_H) / texH;
            float u1 = (float)((gc + 1) * FONT_CHAR_W) / texW;
            float v1 = (float)((gr + 1) * FONT_CHAR_H) / texH;

            // 画面上の四角形座標
            float sx0 = cursorX;
            float sy0 = y;
            float sx1 = cursorX + charDisplayW;
            float sy1 = y + charDisplayH;

            // 頂点を作るヘルパー
            auto makeVert = [&](float vx, float vy, float vu, float vv) -> Vertex3D {
                Vertex3D v;
                v.position = { vx, vy, 0.0f };
                v.normal = { 0.0f, 0.0f, -1.0f };
                v.color = col;
                v.texCoord = { vu, vv };
                return v;
                };

            // 三角形2枚で1文字分の四角形
            verts.push_back(makeVert(sx0, sy0, u0, v0));  // 左上
            verts.push_back(makeVert(sx1, sy0, u1, v0));  // 右上
            verts.push_back(makeVert(sx0, sy1, u0, v1));  // 左下
            verts.push_back(makeVert(sx1, sy0, u1, v0));  // 右上
            verts.push_back(makeVert(sx1, sy1, u1, v1));  // 右下
            verts.push_back(makeVert(sx0, sy1, u0, v1));  // 左下

            cursorX += charDisplayW;
        }

        if (verts.empty()) {
            SetupFor3D();
            m_worldMatrix = savedWorld;
            m_viewMatrix = savedView;
            m_projectionMatrix = savedProj;
            UpdateConstantBuffer();
            return;
        }

        // 動的バッファに書き込む
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(m_pContext->Map(m_pTextVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, verts.data(), verts.size() * sizeof(Vertex3D));
            m_pContext->Unmap(m_pTextVertexBuffer.Get(), 0);
        }

        // マテリアルを白に設定
        struct MaterialData {
            XMFLOAT4 diffuse;
            XMFLOAT4 ambient;
            XMFLOAT4 specular;
            XMFLOAT4 emission;
            float    shininess;
            float    padding[3];
        };
        MaterialData mat = {};
        mat.diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
        m_pContext->UpdateSubresource(m_pMaterialBuffer.Get(), 0, nullptr, &mat, 0, 0);

        // フォントテクスチャをバインド
        ID3D11ShaderResourceView* fontSRV = m_pFontSRV.Get();
        m_pContext->PSSetShaderResources(0, 1, &fontSRV);

        // 一括描画
        UINT stride = sizeof(Vertex3D);
        UINT offset = 0;
        ID3D11Buffer* vb = m_pTextVertexBuffer.Get();
        m_pContext->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pContext->Draw((UINT)verts.size(), 0);

        // 3Dモードに復帰
        SetupFor3D();
        m_worldMatrix = savedWorld;
        m_viewMatrix = savedView;
        m_projectionMatrix = savedProj;
        UpdateConstantBuffer();
    }

} // namespace Engine
