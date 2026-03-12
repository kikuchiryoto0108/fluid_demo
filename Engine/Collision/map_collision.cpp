//==============================================================================
//  File   : map_collision.cpp
//  Brief  : マップコリジョン - 空間分割による静的衝突検出の実装
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "map_collision.h"
#include <cmath>

namespace Engine {

    MapCollision& MapCollision::GetInstance() {
        static MapCollision instance;
        return instance;
    }

    void MapCollision::Initialize(float cellSize) {
        m_cellSize = cellSize;
        m_grid.clear();
        // バッファを事前確保
        m_nearbyBuffer.reserve(64);
        m_penetrationBuffer.reserve(16);
    }

    void MapCollision::Shutdown() {
        m_grid.clear();
        m_nearbyBuffer.clear();
        m_penetrationBuffer.clear();
    }

    void MapCollision::RegisterBlock(BoxCollider* collider) {
        if (!collider) return;

        XMFLOAT3 center = collider->GetCenter();
        int cx, cy, cz;
        GetCellCoord(center, cx, cy, cz);

        int64_t key = GetCellKey(cx, cy, cz);
        m_grid[key].push_back(collider);
    }

    void MapCollision::Clear() {
        m_grid.clear();
    }

    int64_t MapCollision::GetCellKey(int x, int y, int z) const {
        int64_t key = 0;
        key |= (static_cast<int64_t>(x & 0x1FFFFF)) << 42;
        key |= (static_cast<int64_t>(y & 0x1FFFFF)) << 21;
        key |= (static_cast<int64_t>(z & 0x1FFFFF));
        return key;
    }

    void MapCollision::GetCellCoord(const XMFLOAT3& pos, int& outX, int& outY, int& outZ) const {
        outX = static_cast<int>(std::floor(pos.x / m_cellSize));
        outY = static_cast<int>(std::floor(pos.y / m_cellSize));
        outZ = static_cast<int>(std::floor(pos.z / m_cellSize));
    }

    //==========================================================
    // 最適化版: バッファを再利用
    //==========================================================
    void MapCollision::GetNearbyBlocks(const XMFLOAT3& position, float radius, std::vector<BoxCollider*>& outBlocks) {
        outBlocks.clear();

        int centerX, centerY, centerZ;
        GetCellCoord(position, centerX, centerY, centerZ);

        // 検索範囲を最小限に（1セルで十分な場合が多い）
        int cellRadius = static_cast<int>(std::ceil(radius / m_cellSize));
        cellRadius = std::min(cellRadius, 2);  // 最大2セルに制限

        for (int dx = -cellRadius; dx <= cellRadius; ++dx) {
            for (int dy = -cellRadius; dy <= cellRadius; ++dy) {
                for (int dz = -cellRadius; dz <= cellRadius; ++dz) {
                    int64_t key = GetCellKey(centerX + dx, centerY + dy, centerZ + dz);
                    auto it = m_grid.find(key);
                    if (it != m_grid.end()) {
                        outBlocks.insert(outBlocks.end(), it->second.begin(), it->second.end());
                    }
                }
            }
        }
    }

    // 従来版（互換性）
    std::vector<BoxCollider*> MapCollision::GetNearbyBlocks(const XMFLOAT3& position, float radius) {
        GetNearbyBlocks(position, radius, m_nearbyBuffer);
        return m_nearbyBuffer;  // コピーを返す
    }

    bool MapCollision::CheckCollision(BoxCollider* movingCollider, XMFLOAT3& outPenetration) {
        if (!movingCollider) return false;

        XMFLOAT3 center = movingCollider->GetCenter();
        GetNearbyBlocks(center, 2.0f, m_nearbyBuffer);  // 検索範囲を縮小

        for (auto* block : m_nearbyBuffer) {
            if (movingCollider->ComputePenetration(block, outPenetration)) {
                return true;
            }
        }

        outPenetration = { 0.0f, 0.0f, 0.0f };
        return false;
    }

    //==========================================================
    // 最適化版: バッファを再利用
    //==========================================================
    void MapCollision::CheckCollisionAll(BoxCollider* movingCollider, float checkRadius, std::vector<XMFLOAT3>& outPenetrations) {
        outPenetrations.clear();
        if (!movingCollider) return;

        XMFLOAT3 center = movingCollider->GetCenter();
        GetNearbyBlocks(center, checkRadius, m_nearbyBuffer);

        XMFLOAT3 pen;
        for (auto* block : m_nearbyBuffer) {
            if (movingCollider->ComputePenetration(block, pen)) {
                outPenetrations.push_back(pen);
            }
        }
    }

    // 従来版（互換性）
    std::vector<XMFLOAT3> MapCollision::CheckCollisionAll(BoxCollider* movingCollider, float checkRadius) {
        CheckCollisionAll(movingCollider, checkRadius, m_penetrationBuffer);
        return m_penetrationBuffer;  // コピーを返す
    }

} // namespace Engine
