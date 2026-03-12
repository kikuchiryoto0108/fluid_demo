//==============================================================================
//  File   : collision_system.cpp
//  Brief  : コリジョンシステム - 動的オブジェクト間衝突検出の実装
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "collision_system.h"

namespace Engine {

    CollisionSystem& CollisionSystem::GetInstance() {
        static CollisionSystem instance;
        return instance;
    }

    void CollisionSystem::Initialize() {
        m_colliders.clear();
        m_colliders.reserve(64);  // 事前確保
        m_activeBuffer.reserve(64);
        m_nextId = 1;
    }

    void CollisionSystem::Shutdown() {
        m_colliders.clear();
        m_activeBuffer.clear();
        m_callback = nullptr;
    }

    uint32_t CollisionSystem::Register(Collider* collider, CollisionLayer layer, CollisionLayer mask, void* userData) {
        if (!collider) return 0;

        uint32_t id = m_nextId++;
        ColliderData data;
        data.collider = collider;
        data.layer = layer;
        data.mask = mask;
        data.userData = userData;
        data.id = id;
        data.enabled = true;

        m_colliders[id] = data;
        return id;
    }

    void CollisionSystem::Unregister(uint32_t id) {
        m_colliders.erase(id);
    }

    void CollisionSystem::SetEnabled(uint32_t id, bool enabled) {
        auto it = m_colliders.find(id);
        if (it != m_colliders.end()) {
            it->second.enabled = enabled;
        }
    }

    void CollisionSystem::Update() {
        if (!m_callback) return;

        // バッファを再利用
        m_activeBuffer.clear();
        for (auto& pair : m_colliders) {
            if (pair.second.enabled && pair.second.collider) {
                m_activeBuffer.push_back(&pair.second);
            }
        }

        const size_t count = m_activeBuffer.size();

        // 少数のオブジェクトなら全ペアチェック
        for (size_t i = 0; i < count; ++i) {
            for (size_t j = i + 1; j < count; ++j) {
                ColliderData* a = m_activeBuffer[i];
                ColliderData* b = m_activeBuffer[j];

                // レイヤーマスクチェック（早期終了）
                if (!HasFlag(a->mask, b->layer) || !HasFlag(b->mask, a->layer)) {
                    continue;
                }

                // 衝突判定
                if (a->collider->Intersects(b->collider)) {
                    CollisionHit hit;
                    hit.dataA = a;
                    hit.dataB = b;

                    if (a->collider->GetType() == ColliderType::BOX &&
                        b->collider->GetType() == ColliderType::BOX) {
                        static_cast<BoxCollider*>(a->collider)->ComputePenetration(
                            static_cast<BoxCollider*>(b->collider), hit.penetration);
                    }

                    m_callback(hit);
                }
            }
        }
    }

} // namespace Engine
