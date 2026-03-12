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
    //==========================================================
    class MapCollision {
    public:
        static MapCollision& GetInstance();

        void Initialize(float cellSize = 2.0f);
        void Shutdown();

        void RegisterBlock(BoxCollider* collider);
        void Clear();

        // 最適化: 結果を参照で受け取る版
        void GetNearbyBlocks(const XMFLOAT3& position, float radius, std::vector<BoxCollider*>& outBlocks);

        // 従来版（互換性のため残す）
        std::vector<BoxCollider*> GetNearbyBlocks(const XMFLOAT3& position, float radius);

        bool CheckCollision(BoxCollider* movingCollider, XMFLOAT3& outPenetration);

        // 最適化: 結果を参照で受け取る版
        void CheckCollisionAll(BoxCollider* movingCollider, float checkRadius, std::vector<XMFLOAT3>& outPenetrations);

        // 従来版（互換性のため残す）
        std::vector<XMFLOAT3> CheckCollisionAll(BoxCollider* movingCollider, float checkRadius = 3.0f);

    private:
        MapCollision() = default;

        int64_t GetCellKey(int x, int y, int z) const;
        void GetCellCoord(const XMFLOAT3& pos, int& outX, int& outY, int& outZ) const;

        float m_cellSize = 2.0f;
        std::unordered_map<int64_t, std::vector<BoxCollider*>> m_grid;

        // 最適化: 再利用可能なワークバッファ
        mutable std::vector<BoxCollider*> m_nearbyBuffer;
        mutable std::vector<XMFLOAT3> m_penetrationBuffer;
    };

} // namespace Engine
