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

    class CollisionManager {
    public:
        static CollisionManager& GetInstance() {
            static CollisionManager instance;
            return instance;
        }

        void Initialize(float mapCellSize = 2.0f) {
            CollisionSystem::GetInstance().Initialize();
            MapCollision::GetInstance().Initialize(mapCellSize);
            m_penetrationBuffer.reserve(16);
        }

        void Shutdown() {
            CollisionSystem::GetInstance().Shutdown();
            MapCollision::GetInstance().Shutdown();
        }

        void Update() {
            CollisionSystem::GetInstance().Update();
        }

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

        void RegisterMapBlock(BoxCollider* block) {
            MapCollision::GetInstance().RegisterBlock(block);
        }

        bool CheckMapCollision(BoxCollider* collider, DirectX::XMFLOAT3& penetration) {
            return MapCollision::GetInstance().CheckCollision(collider, penetration);
        }

        //==========================================================
        // ResolveMapCollision - 最適化版
        //==========================================================
        bool ResolveMapCollision(BoxCollider* collider, DirectX::XMFLOAT3& outPosition,
            DirectX::XMFLOAT3& outVelocity, bool& outGrounded) {
            // バッファを再利用
            MapCollision::GetInstance().CheckCollisionAll(collider, 2.0f, m_penetrationBuffer);

            outGrounded = false;

            if (m_penetrationBuffer.empty()) {
                return false;
            }

            for (const auto& pen : m_penetrationBuffer) {
                outPosition.x += pen.x;
                outPosition.y += pen.y;
                outPosition.z += pen.z;

                if (pen.x != 0.0f) outVelocity.x = 0.0f;
                if (pen.y != 0.0f) {
                    outVelocity.y = 0.0f;
                    if (pen.y > 0.0f) outGrounded = true;
                }
                if (pen.z != 0.0f) outVelocity.z = 0.0f;
            }

            return true;
        }

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

        // 再利用バッファ
        std::vector<DirectX::XMFLOAT3> m_penetrationBuffer;
    };

} // namespace Engine
