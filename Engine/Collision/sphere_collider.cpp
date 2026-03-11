//==============================================================================
//  File   : sphere_collider.cpp
//  Brief  : 球体コライダー - 球形衝突判定の実装
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "sphere_collider.h"
#include "box_collider.h"
#include <algorithm>
#include <cmath>

namespace Engine {

    //==========================================================
    // コンストラクタ
    //==========================================================
    SphereCollider::SphereCollider() : m_radius(1.0f), m_worldRadius(1.0f) {
        m_position = { 0.0f, 0.0f, 0.0f };
    }

    SphereCollider::SphereCollider(const XMFLOAT3& center, float radius) : m_radius(radius) {
        m_position = center;
        UpdateWorldRadius();
    }

    //==========================================================
    // 設定メソッド
    //==========================================================
    void SphereCollider::SetCenter(const XMFLOAT3& center) {
        m_position = center;
    }

    void SphereCollider::SetRadius(float radius) {
        m_radius = radius;
        UpdateWorldRadius();
    }

    //==========================================================
    // UpdateWorldRadius - スケールを考慮したワールド半径の更新
    //==========================================================
    void SphereCollider::UpdateWorldRadius() {
        // スケールの最大値を使用して均一なスケーリング
        float maxScale = std::max({ m_scale.x, m_scale.y, m_scale.z });
        m_worldRadius = m_radius * maxScale;
    }

    //==========================================================
    // 取得メソッド
    //==========================================================
    XMFLOAT3 SphereCollider::GetCenter() const {
        return m_position;
    }

    void SphereCollider::GetBounds(XMFLOAT3& outMin, XMFLOAT3& outMax) const {
        // AABBバウンディングボックスを計算
        outMin = { m_position.x - m_worldRadius, m_position.y - m_worldRadius, m_position.z - m_worldRadius };
        outMax = { m_position.x + m_worldRadius, m_position.y + m_worldRadius, m_position.z + m_worldRadius };
    }

    //==========================================================
    // Intersects - 他のコライダーとの衝突判定
    //==========================================================
    bool SphereCollider::Intersects(const Collider* other) const {
        if (!other) return false;

        switch (other->GetType()) {
        case ColliderType::SPHERE: {
            // --- 球体同士の衝突判定 ---
            const auto* s = static_cast<const SphereCollider*>(other);
            float dx = m_position.x - s->m_position.x;
            float dy = m_position.y - s->m_position.y;
            float dz = m_position.z - s->m_position.z;
            float distSq = dx * dx + dy * dy + dz * dz;
            float radiusSum = m_worldRadius + s->m_worldRadius;
            return distSq <= radiusSum * radiusSum;
        }
        case ColliderType::BOX:
            // ボックスコライダー側の判定を使用
            return other->Intersects(this);
        default:
            return false;
        }
    }

} // namespace Engine
