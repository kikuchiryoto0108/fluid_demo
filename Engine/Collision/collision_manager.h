//==============================================================================
//  File   : collision_manager.h
//  Brief  : コリジョンマネージャー - 衝突システムの統合管理
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include "collision_system.h"
#include "map_collision.h"
#include "collider.h"
#include <DirectXMath.h>

namespace Engine {

//==========================================================
// CollisionManager - 衝突システムの一元管理クラス
// CollisionSystemとMapCollisionへの統合アクセスを提供
// シングルトンパターンで実装
//==========================================================
class CollisionManager {
public:
    //==========================================================
    // シングルトンインスタンス取得
    //==========================================================
    static CollisionManager& GetInstance() {
        static CollisionManager instance;
        return instance;
    }

    //==========================================================
    // 初期化・終了処理
    //==========================================================
    void Initialize(float mapCellSize = 2.0f) {
        CollisionSystem::GetInstance().Initialize();
        MapCollision::GetInstance().Initialize(mapCellSize);
    }

    void Shutdown() {
        CollisionSystem::GetInstance().Shutdown();
        MapCollision::GetInstance().Shutdown();
    }

    void Update() {
        CollisionSystem::GetInstance().Update();
    }

    //==========================================================
    // 動的オブジェクト管理
    //==========================================================
    uint32_t RegisterDynamic(Collider* collider, CollisionLayer layer,
                      CollisionLayer mask, void* userData) {
        return CollisionSystem::GetInstance().Register(collider, layer, mask, userData);
    }

    void UnregisterDynamic(uint32_t id) {
        CollisionSystem::GetInstance().Unregister(id);
    }

    void SetEnabled(uint32_t id, bool enabled) {
        CollisionSystem::GetInstance().SetEnabled(id, enabled);
    }

    void SetCallback(CollisionCallback callback) {
        CollisionSystem::GetInstance().SetCallback(std::move(callback));
    }

    //==========================================================
    // マップコリジョン操作
    //==========================================================
    void RegisterMapBlock(BoxCollider* block) {
        MapCollision::GetInstance().RegisterBlock(block);
    }

    bool CheckMapCollision(BoxCollider* collider, DirectX::XMFLOAT3& penetration) {
        return MapCollision::GetInstance().CheckCollision(collider, penetration);
    }

    std::vector<DirectX::XMFLOAT3> CheckMapCollisionAll(BoxCollider* collider, float radius) {
        return MapCollision::GetInstance().CheckCollisionAll(collider, radius);
    }

    //==========================================================
    // ResolveMapCollision - マップとの衝突解決
    //==========================================================
    bool ResolveMapCollision(BoxCollider* collider, DirectX::XMFLOAT3& outPosition,
                             DirectX::XMFLOAT3& outVelocity, bool& outGrounded) {
        auto penetrations = MapCollision::GetInstance().CheckCollisionAll(collider, 3.0f);
        outGrounded = false;
        for (const auto& pen : penetrations) {
            // --- 位置の補正 ---
            outPosition.x += pen.x;
            outPosition.y += pen.y;
            outPosition.z += pen.z;
            // --- 速度のリセット ---
            if (pen.x != 0.0f) outVelocity.x = 0.0f;
            if (pen.y != 0.0f) {
                outVelocity.y = 0.0f;
                if (pen.y > 0.0f) outGrounded = true;  // 地面に接地
            }
            if (pen.z != 0.0f) outVelocity.z = 0.0f;
        }
        return !penetrations.empty();
    }

    //==========================================================
    // 内部システムへの直接アクセス（互換性のため）
    //==========================================================
    CollisionSystem& GetCollisionSystem() {
        return CollisionSystem::GetInstance();
    }

    MapCollision& GetMapCollision() {
        return MapCollision::GetInstance();
    }

private:
    CollisionManager() = default;
    ~CollisionManager() = default;
    CollisionManager(const CollisionManager&) = delete;
    CollisionManager& operator=(const CollisionManager&) = delete;
};

} // namespace Engine
