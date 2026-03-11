//==============================================================================
//  File   : primitive.h
//  Brief  : プリミティブ - 基本形状の頂点データとデフォルトテクスチャ
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include "Engine/Core/renderer.h"
#include "vertex.h"

namespace Engine {
    // --- 立方体の頂点データ（36頂点） ---
    extern Vertex3D BoxVertices[36];

    // --- プリミティブ初期化・終了 ---
    void InitPrimitives(ID3D11Device* pDevice);
    void UninitPrimitives();

    // --- デフォルトテクスチャ取得 ---
    ID3D11ShaderResourceView* GetDefaultTexture();
}

//==========================================================
// グローバル公開（旧互換）
//==========================================================
extern Engine::Vertex3D Box[36];
inline void InitPolygon() { Engine::InitPrimitives(Engine::GetDevice()); }
inline void UninitPolygon() { Engine::UninitPrimitives(); }
inline ID3D11ShaderResourceView* GetPolygonTexture() { return Engine::GetDefaultTexture(); }
