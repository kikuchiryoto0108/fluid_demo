//==============================================================================
//  File   : texture_loader.h
//  Brief  : テクスチャローダー - 画像ファイルからテクスチャを読み込む
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include <d3d11.h>
#include <string>

namespace Engine {

    //==========================================================
    // テクスチャローダークラス
    //==========================================================
    class TextureLoader {
    public:
        // --- ファイルからテクスチャ読み込み ---
        static ID3D11ShaderResourceView* Load(
            ID3D11Device* pDevice,
            const std::wstring& filePath
        );

        // --- 1x1白テクスチャ生成（フォールバック用） ---
        static ID3D11ShaderResourceView* CreateWhite(ID3D11Device* pDevice);
    };
}
