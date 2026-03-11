//==============================================================================
//  File   : map_renderer.h
//  Brief  : マップ描画システム - 3Dブロックの描画処理
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include "main.h"
#include "Engine/Core/renderer.h"
#include "Engine/Graphics/vertex.h"
#include "Engine/Graphics/material.h"
#include "map.h"

namespace Game {

//==========================================================
// マクロ定義
//==========================================================
#define BOX_SIZE 1.0f  // 1つのボックスのサイズ

//==========================================================
// マップレンダラークラス
//==========================================================
class MapRenderer
{
private:
    // --- 直方体の頂点バッファ ---
    ID3D11Buffer* m_VertexBuffer;

    // --- インデックスバッファ ---
    ID3D11Buffer* m_IndexBuffer;

    // --- テクスチャリソース ---
    ID3D11ShaderResourceView* m_Texture;

    // --- マップデータへの参照 ---
    Map* m_pMap;

    // --- 直方体の頂点・インデックスバッファを作成 ---
    void CreateCubeBuffers();

public:
    // --- コンストラクタ・デストラクタ ---
    MapRenderer();
    ~MapRenderer();

    // --- 初期化・終了処理 ---
    HRESULT Initialize(Map* pMap);
    void Uninitialize();

    // --- 描画処理 ---
    void Draw();

    // --- 単一のボックス描画 ---
    void DrawBox(float x, float y, float z);

    // --- ワールド座標の計算 ---
    XMFLOAT3 GetWorldPosition(int mapX, int mapY, int mapZ) const;
};

} // namespace Game
