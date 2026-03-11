//==============================================================================
//  File   : vertex.h
//  Brief  : 頂点構造体 - 各種頂点フォーマット定義
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include <DirectXMath.h>

namespace Engine {
    using namespace DirectX;

    //==========================================================
    // 基本3D頂点構造体
    //==========================================================
    struct Vertex3D {
        XMFLOAT3 position;   // 座標
        XMFLOAT3 normal;     // 法線
        XMFLOAT4 color;      // 頂点カラー
        XMFLOAT2 texCoord;   // テクスチャ座標

        Vertex3D()
            : position(0.0f, 0.0f, 0.0f)
            , normal(0.0f, 0.0f, 0.0f)
            , color(1.0f, 1.0f, 1.0f, 1.0f)
            , texCoord(0.0f, 0.0f) {
        }

        Vertex3D(const XMFLOAT3& pos, const XMFLOAT3& norm,
            const XMFLOAT4& col, const XMFLOAT2& uv)
            : position(pos)
            , normal(norm)
            , color(col)
            , texCoord(uv) {
        }
    };

    //==========================================================
    // スキンメッシュ用頂点構造体（将来用）
    //==========================================================
    struct VertexSkinned {
        XMFLOAT3 position;       // 座標
        XMFLOAT3 normal;         // 法線
        XMFLOAT4 color;          // 頂点カラー
        XMFLOAT2 texCoord;       // テクスチャ座標
        uint32_t boneIndices[4]; // ボーンインデックス
        float boneWeights[4];    // ボーンウェイト
    };

    //==========================================================
    // 2D頂点構造体（スプライト用）
    //==========================================================
    struct Vertex2D {
        XMFLOAT3 position;   // 座標
        XMFLOAT4 color;      // 頂点カラー
        XMFLOAT2 texCoord;   // テクスチャ座標
    };
}
