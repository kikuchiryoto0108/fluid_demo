//==============================================================================
//  File   : sprite_2d.h
//  Brief  : 2Dスプライト - スクリーン座標での板ポリゴン描画
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include "vertex.h"
#include <d3d11.h>
#include <DirectXMath.h>

namespace Engine {
    using namespace DirectX;

    //==========================================================
    // 2Dスプライトクラス
    //==========================================================
    class Sprite2D {
    public:
        // --- 初期化・終了 ---
        static bool Initialize(ID3D11Device* pDevice);
        static void Finalize();

        // --- 描画 ---
        static void Draw(
            ID3D11DeviceContext* pContext,
            ID3D11Buffer* pMaterialBuffer,
            const XMFLOAT2& position,           // スクリーン座標
            const XMFLOAT2& size,               // サイズ
            const XMFLOAT4& color,              // 頂点カラー
            ID3D11ShaderResourceView* pTexture = nullptr,  // テクスチャ
            float rotation = 0.0f               // 回転（ラジアン）
        );

    private:
        static ID3D11Buffer* s_pVertexBuffer;   // 頂点バッファ
    };
}
