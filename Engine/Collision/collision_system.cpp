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

    //==========================================================
    // シングルトンインスタンス取得
    //==========================================================
    CollisionSystem& CollisionSystem::GetInstance() {
        static CollisionSystem instance;
        return instance;
    }

    //==========================================================
    // Initialize - システム初期化
    //==========================================================
    void CollisionSystem::Initialize() {
        m_colliders.clear();
        m_nextId = 1;
    }

    //==========================================================
    // Shutdown - システム終了
    //==========================================================
    void CollisionSystem::Shutdown() {
        m_colliders.clear();
        m_callback = nullptr;
    }

    //==========================================================
    // Register - コライダーの登録
    //==========================================================
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

    //==========================================================
    // Unregister - コライダーの登録解除
    //==========================================================
    void CollisionSystem::Unregister(uint32_t id) {
        m_colliders.erase(id);
    }

    //==========================================================
    // SetEnabled - コライダーの有効/無効切り替え
    //==========================================================
    void CollisionSystem::SetEnabled(uint32_t id, bool enabled) {
        auto it = m_colliders.find(id);
        if (it != m_colliders.end()) {
            it->second.enabled = enabled;
        }
    }

    //==========================================================
    // Update - 衝突検出の更新処理
    //==========================================================
    void CollisionSystem::Update() {
        if (!m_callback) return;

        // --- 有効なコライダーを収集 ---
        std::vector<ColliderData*> active;
        for (auto& pair : m_colliders) {
            if (pair.second.enabled && pair.second.collider) {
                active.push_back(&pair.second);
            }
        }

        // --- 全ペアの衝突チェック（ブロードフェーズなし） ---
        for (size_t i = 0; i < active.size(); ++i) {
            for (size_t j = i + 1; j < active.size(); ++j) {
                ColliderData* a = active[i];
                ColliderData* b = active[j];

                // レイヤーマスクチェック
                if (!HasFlag(a->mask, b->layer)) continue;
                if (!HasFlag(b->mask, a->layer)) continue;

                // 衝突判定
                if (a->collider->Intersects(b->collider)) {
                    CollisionHit hit;
                    hit.dataA = a;
                    hit.dataB = b;

                    // ボックス同士の場合はめり込み量を計算
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
