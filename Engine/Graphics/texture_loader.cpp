//==============================================================================
//  File   : texture_loader.cpp
//  Brief  : テクスチャローダー - 画像ファイルからテクスチャを読み込む実装
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "texture_loader.h"
#include "external/WICTextureLoader11.h"
#include "Engine/Core/debug_log.h"

// Engine名前空間のDebugLog/DebugLogWを使用
using Engine::DebugLog;
using Engine::DebugLogW;

namespace Engine {

    //==========================================================
    // ファイルからテクスチャ読み込み
    //==========================================================
    ID3D11ShaderResourceView* TextureLoader::Load(
        ID3D11Device* pDevice,
        const std::wstring& filePath) {
        if (!pDevice) {
            DebugLog("[TextureLoader::Load] FAILED: pDevice is null\n");
            return nullptr;
        }

        DebugLogW(L"[TextureLoader::Load] Loading: %s\n", filePath.c_str());

        ID3D11ShaderResourceView* pTexture = nullptr;
        HRESULT hr = DirectX::CreateWICTextureFromFile(
            pDevice,
            filePath.c_str(),
            nullptr,
            &pTexture
        );

        if (FAILED(hr)) {
            // 読み込み失敗時は白テクスチャでフォールバック
            DebugLog("[TextureLoader::Load] FAILED to load texture, HRESULT=0x%08X, using white fallback\n", hr);
            return CreateWhite(pDevice);
        }

        DebugLog("[TextureLoader::Load] SUCCESS\n");
        return pTexture;
    }

    //==========================================================
    // 1x1白テクスチャ生成（フォールバック用）
    //==========================================================
    ID3D11ShaderResourceView* TextureLoader::CreateWhite(ID3D11Device* pDevice) {
        if (!pDevice) {
            DebugLog("[TextureLoader::CreateWhite] FAILED: pDevice is null\n");
            return nullptr;
        }

        DebugLog("[TextureLoader::CreateWhite] Creating 1x1 white texture\n");

        // --- テクスチャ記述子 ---
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = 1;
        desc.Height = 1;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        // --- 白ピクセルデータ ---
        uint32_t white = 0xFFFFFFFF;
        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = &white;
        initData.SysMemPitch = sizeof(uint32_t);

        // --- テクスチャ作成 ---
        ID3D11Texture2D* pTex = nullptr;
        HRESULT hr = pDevice->CreateTexture2D(&desc, &initData, &pTex);
        if (FAILED(hr)) {
            DebugLog("[TextureLoader::CreateWhite] FAILED to create texture2D, HRESULT=0x%08X\n", hr);
            return nullptr;
        }

        // --- シェーダーリソースビュー作成 ---
        ID3D11ShaderResourceView* pSRV = nullptr;
        hr = pDevice->CreateShaderResourceView(pTex, nullptr, &pSRV);
        pTex->Release();

        if (FAILED(hr)) {
            DebugLog("[TextureLoader::CreateWhite] FAILED to create SRV, HRESULT=0x%08X\n", hr);
            return nullptr;
        }

        DebugLog("[TextureLoader::CreateWhite] SUCCESS\n");
        return pSRV;
    }
}
