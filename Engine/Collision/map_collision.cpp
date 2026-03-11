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

    //==========================================================
    // シングルトンインスタンス取得
    //==========================================================
    MapCollision& MapCollision::GetInstance() {
        static MapCollision instance;
        return instance;
    }

    //==========================================================
    // Initialize - システム初期化
    //==========================================================
    void MapCollision::Initialize(float cellSize) {
        m_cellSize = cellSize;
        m_grid.clear();
    }

    //==========================================================
    // Shutdown - システム終了
    //==========================================================
    void MapCollision::Shutdown() {
        m_grid.clear();
    }

    //==========================================================
    // RegisterBlock - 静的ブロックをグリッドに登録
    //==========================================================
    void MapCollision::RegisterBlock(BoxCollider* collider) {
        if (!collider) return;

        XMFLOAT3 center = collider->GetCenter();
        int cx, cy, cz;
        GetCellCoord(center, cx, cy, cz);

        int64_t key = GetCellKey(cx, cy, cz);
        m_grid[key].push_back(collider);
    }

    //==========================================================
    // Clear - グリッドのクリア
    //==========================================================
    void MapCollision::Clear() {
        m_grid.clear();
    }

    //==========================================================
    // GetCellKey - 3Dセル座標から64ビットキーを生成
    //==========================================================
    int64_t MapCollision::GetCellKey(int x, int y, int z) const {
        // 各軸21ビットで64ビットキーにパック
        int64_t key = 0;
        key |= (static_cast<int64_t>(x & 0x1FFFFF)) << 42;
        key |= (static_cast<int64_t>(y & 0x1FFFFF)) << 21;
        key |= (static_cast<int64_t>(z & 0x1FFFFF));
        return key;
    }

    //==========================================================
    // GetCellCoord - ワールド座標からセル座標を計算
    //==========================================================
    void MapCollision::GetCellCoord(const XMFLOAT3& pos, int& outX, int& outY, int& outZ) const {
        outX = static_cast<int>(std::floor(pos.x / m_cellSize));
        outY = static_cast<int>(std::floor(pos.y / m_cellSize));
        outZ = static_cast<int>(std::floor(pos.z / m_cellSize));
    }

    //==========================================================
    // GetNearbyBlocks - 指定位置周辺のブロックを取得
    //==========================================================
    std::vector<BoxCollider*> MapCollision::GetNearbyBlocks(const XMFLOAT3& position, float radius) {
        std::vector<BoxCollider*> result;

        int centerX, centerY, centerZ;
        GetCellCoord(position, centerX, centerY, centerZ);

        // 検索範囲をセル数に変換
        int cellRadius = static_cast<int>(std::ceil(radius / m_cellSize)) + 1;

        // --- 近傍セルを走査 ---
        for (int dx = -cellRadius; dx <= cellRadius; ++dx) {
            for (int dy = -cellRadius; dy <= cellRadius; ++dy) {
                for (int dz = -cellRadius; dz <= cellRadius; ++dz) {
                    int64_t key = GetCellKey(centerX + dx, centerY + dy, centerZ + dz);
                    auto it = m_grid.find(key);
                    if (it != m_grid.end()) {
                        for (auto* block : it->second) {
                            result.push_back(block);
                        }
                    }
                }
            }
        }

        return result;
    }

    //==========================================================
    // CheckCollision - 最初に見つかった衝突を返す
    //==========================================================
    bool MapCollision::CheckCollision(BoxCollider* movingCollider, XMFLOAT3& outPenetration) {
        if (!movingCollider) return false;

        XMFLOAT3 center = movingCollider->GetCenter();
        auto nearby = GetNearbyBlocks(center, 3.0f);

        for (auto* block : nearby) {
            if (movingCollider->ComputePenetration(block, outPenetration)) {
                return true;
            }
        }

        outPenetration = { 0.0f, 0.0f, 0.0f };
        return false;
    }

    //==========================================================
    // CheckCollisionAll - 全ての衝突のめり込み量を返す
    //==========================================================
    std::vector<XMFLOAT3> MapCollision::CheckCollisionAll(BoxCollider* movingCollider, float checkRadius) {
        std::vector<XMFLOAT3> penetrations;
        if (!movingCollider) return penetrations;

        XMFLOAT3 center = movingCollider->GetCenter();
        auto nearby = GetNearbyBlocks(center, checkRadius);

        for (auto* block : nearby) {
            XMFLOAT3 pen;
            if (movingCollider->ComputePenetration(block, pen)) {
                penetrations.push_back(pen);
            }
        }

        return penetrations;
    }

} // namespace Engine
