//==============================================================================
//  File   : map.h
//  Brief  : 3次元マップデータ管理 - ブロックベースのマップシステム
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include "main.h"
#include <vector>
#include <memory>
#include "Game/Objects/game_object.h"

namespace Game {

//==========================================================
// マクロ定義
//==========================================================
#define MAP_WIDTH  50    // マップの幅
#define MAP_HEIGHT 50    // マップの高さ
#define MAP_DEPTH  50    // マップの奥行き

//==========================================================
// 構造体の定義
//==========================================================
struct MapPosition
{
    int x, y, z;
};

//==========================================================
// マップクラス
//==========================================================
class Map
{
private:
    // --- 3次元マップデータ配列 [高さ][奥行き][幅] ---
    int mapData[MAP_HEIGHT][MAP_DEPTH][MAP_WIDTH];
    std::vector<std::shared_ptr<GameObject>> blockObjects;  // ブロックGameObjectリスト

public:
    // --- コンストラクタ・デストラクタ ---
    Map();
    ~Map();

    // --- 初期化・終了処理 ---
    HRESULT Initialize(ID3D11ShaderResourceView* texture);
    void Uninitialize();

    // --- マップデータアクセス ---
    int GetBlock(int x, int y, int z) const;
    void SetBlock(int x, int y, int z, int value);

    // --- 範囲チェック ---
    bool IsValidPosition(int x, int y, int z) const;

    // --- マップサイズ取得 ---
    int GetWidth() const { return MAP_WIDTH; }
    int GetHeight() const { return MAP_HEIGHT; }
    int GetDepth() const { return MAP_DEPTH; }

    // --- サンプルマップデータの設定 ---
    void CreateSampleMap();

    // --- ブロックGameObjectリスト取得 ---
    const std::vector<std::shared_ptr<GameObject>>& GetBlockObjects() const { return blockObjects; }

    // --- 地面の高さを取得 ---
    float GetGroundHeight(float x, float z) const;
   
    // --- ブロックGameObject生成 ---
    void GenerateBlockObjects(ID3D11ShaderResourceView* texture);
};

} // namespace Game
