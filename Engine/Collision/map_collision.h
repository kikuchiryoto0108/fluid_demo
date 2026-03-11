//==============================================================================
//  File   : map_collision.h
//  Brief  : マップコリジョン - 空間分割による静的オブジェクト衝突検出
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include "box_collider.h"
#include <vector>
#include <unordered_map>

namespace Engine {

    //==========================================================
    // MapCollision - マップ衝突システム（シングルトン）
    // 空間ハッシュグリッドを使用した効率的な衝突検出
    //==========================================================
    class MapCollision {
    public:
        static MapCollision& GetInstance();

        // --- 初期化・終了 ---
        void Initialize(float cellSize = 2.0f);
        void Shutdown();

        // --- ブロック管理 ---
        void RegisterBlock(BoxCollider* collider);
        void Clear();

        // --- 近傍検索 ---
        std::vector<BoxCollider*> GetNearbyBlocks(const XMFLOAT3& position, float radius);

        // --- 衝突判定 ---
        bool CheckCollision(BoxCollider* movingCollider, XMFLOAT3& outPenetration);
        std::vector<XMFLOAT3> CheckCollisionAll(BoxCollider* movingCollider, float checkRadius = 3.0f);

    private:
        MapCollision() = default;

        // --- 空間ハッシュ計算 ---
        int64_t GetCellKey(int x, int y, int z) const;
        void GetCellCoord(const XMFLOAT3& pos, int& outX, int& outY, int& outZ) const;

        // --- メンバ変数 ---
        float m_cellSize = 2.0f;  // グリッドセルサイズ
        std::unordered_map<int64_t, std::vector<BoxCollider*>> m_grid;  // 空間ハッシュグリッド
    };

} // namespace Engine
