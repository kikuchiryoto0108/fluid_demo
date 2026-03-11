//==============================================================================
//  File   : box_collider.cpp
//  Brief  : ボックスコライダー - AABB衝突判定の実装
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "box_collider.h"
#include "sphere_collider.h"
#include <algorithm>
#include <cmath>

namespace Engine {

    //==========================================================
    // コンストラクタ
    //==========================================================
    BoxCollider::BoxCollider() : m_size(1.0f, 1.0f, 1.0f) {
        m_position = { 0.0f, 0.0f, 0.0f };
        UpdateWorldBounds();
    }

    BoxCollider::BoxCollider(const XMFLOAT3& center, const XMFLOAT3& size) : m_size(size) {
        m_position = center;
        UpdateWorldBounds();
    }

    //==========================================================
    // コピーコンストラクタ・代入演算子
    //==========================================================
    BoxCollider::BoxCollider(const BoxCollider& other)
        : m_size(other.m_size)
        , m_worldMin(other.m_worldMin)
        , m_worldMax(other.m_worldMax) {
        m_position = other.m_position;
        m_rotation = other.m_rotation;
        m_scale = other.m_scale;
    }

    BoxCollider& BoxCollider::operator=(const BoxCollider& other) {
        if (this != &other) {
            m_position = other.m_position;
            m_rotation = other.m_rotation;
            m_scale = other.m_scale;
            m_size = other.m_size;
            m_worldMin = other.m_worldMin;
            m_worldMax = other.m_worldMax;
        }
        return *this;
    }

    //==========================================================
    // 設定メソッド
    //==========================================================
    void BoxCollider::SetCenter(const XMFLOAT3& center) {
        m_position = center;
        UpdateWorldBounds();
    }

    void BoxCollider::SetSize(const XMFLOAT3& size) {
        m_size = size;
        UpdateWorldBounds();
    }

    void BoxCollider::SetTransform(const XMFLOAT3& position, const XMFLOAT3& rotation, const XMFLOAT3& scale) {
        m_position = position;
        m_rotation = rotation;
        m_scale = scale;
        UpdateWorldBounds();
    }

    //==========================================================
    // UpdateWorldBounds - ワールド空間のバウンディングボックス更新
    //==========================================================
    void BoxCollider::UpdateWorldBounds() {
        // スケールを適用したハーフサイズを計算
        float halfX = m_size.x * m_scale.x * 0.5f;
        float halfY = m_size.y * m_scale.y * 0.5f;
        float halfZ = m_size.z * m_scale.z * 0.5f;

        m_worldMin = { m_position.x - halfX, m_position.y - halfY, m_position.z - halfZ };
        m_worldMax = { m_position.x + halfX, m_position.y + halfY, m_position.z + halfZ };
    }

    //==========================================================
    // 取得メソッド
    //==========================================================
    XMFLOAT3 BoxCollider::GetCenter() const {
        return m_position;
    }

    void BoxCollider::GetBounds(XMFLOAT3& outMin, XMFLOAT3& outMax) const {
        outMin = m_worldMin;
        outMax = m_worldMax;
    }

    //==========================================================
    // Intersects - 他のコライダーとの衝突判定
    //==========================================================
    bool BoxCollider::Intersects(const Collider* other) const {
        if (!other) return false;

        switch (other->GetType()) {
        case ColliderType::BOX:
            return IntersectsBox(static_cast<const BoxCollider*>(other));
        case ColliderType::SPHERE:
            return IntersectsSphere(static_cast<const SphereCollider*>(other));
        default:
            return false;
        }
    }

    //==========================================================
    // IntersectsBox - ボックス同士のAABB衝突判定
    //==========================================================
    bool BoxCollider::IntersectsBox(const BoxCollider* other) const {
        // 各軸で重なりをチェック
        return (m_worldMin.x <= other->m_worldMax.x && m_worldMax.x >= other->m_worldMin.x) &&
            (m_worldMin.y <= other->m_worldMax.y && m_worldMax.y >= other->m_worldMin.y) &&
            (m_worldMin.z <= other->m_worldMax.z && m_worldMax.z >= other->m_worldMin.z);
    }

    //==========================================================
    // IntersectsSphere - ボックスと球体の衝突判定
    //==========================================================
    bool BoxCollider::IntersectsSphere(const SphereCollider* other) const {
        if (!other) return false;

        XMFLOAT3 center = other->GetCenter();
        float radius = other->GetWorldRadius();

        // ボックス上の最近点を計算
        float closestX = std::clamp(center.x, m_worldMin.x, m_worldMax.x);
        float closestY = std::clamp(center.y, m_worldMin.y, m_worldMax.y);
        float closestZ = std::clamp(center.z, m_worldMin.z, m_worldMax.z);

        // 最近点と球体中心の距離をチェック
        float dx = center.x - closestX;
        float dy = center.y - closestY;
        float dz = center.z - closestZ;

        return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
    }

    //==========================================================
    // Contains - 点がボックス内に含まれるか判定
    //==========================================================
    bool BoxCollider::Contains(const XMFLOAT3& point) const {
        return (point.x >= m_worldMin.x && point.x <= m_worldMax.x) &&
            (point.y >= m_worldMin.y && point.y <= m_worldMax.y) &&
            (point.z >= m_worldMin.z && point.z <= m_worldMax.z);
    }

    //==========================================================
    // ComputePenetration - めり込み量の計算（最小軸分離）
    //==========================================================
    bool BoxCollider::ComputePenetration(const BoxCollider* other, XMFLOAT3& outPenetration) const {
        if (!IntersectsBox(other)) {
            outPenetration = { 0.0f, 0.0f, 0.0f };
            return false;
        }

        // 各軸のめり込み量を計算
        float overlapX = std::min(m_worldMax.x - other->m_worldMin.x, other->m_worldMax.x - m_worldMin.x);
        float overlapY = std::min(m_worldMax.y - other->m_worldMin.y, other->m_worldMax.y - m_worldMin.y);
        float overlapZ = std::min(m_worldMax.z - other->m_worldMin.z, other->m_worldMax.z - m_worldMin.z);

        outPenetration = { 0.0f, 0.0f, 0.0f };

        // 最小のめり込み軸で押し出し方向を決定
        if (overlapX <= overlapY && overlapX <= overlapZ) {
            float myCenter = (m_worldMin.x + m_worldMax.x) * 0.5f;
            float otherCenter = (other->m_worldMin.x + other->m_worldMax.x) * 0.5f;
            outPenetration.x = (myCenter < otherCenter) ? -overlapX : overlapX;
        } else if (overlapY <= overlapZ) {
            float myCenter = (m_worldMin.y + m_worldMax.y) * 0.5f;
            float otherCenter = (other->m_worldMin.y + other->m_worldMax.y) * 0.5f;
            outPenetration.y = (myCenter < otherCenter) ? -overlapY : overlapY;
        } else {
            float myCenter = (m_worldMin.z + m_worldMax.z) * 0.5f;
            float otherCenter = (other->m_worldMin.z + other->m_worldMax.z) * 0.5f;
            outPenetration.z = (myCenter < otherCenter) ? -overlapZ : overlapZ;
        }

        return true;
    }

    //==========================================================
    // Create - ファクトリメソッド
    //==========================================================
    std::unique_ptr<BoxCollider> BoxCollider::Create(const XMFLOAT3& center, const XMFLOAT3& size) {
        return std::make_unique<BoxCollider>(center, size);
    }

} // namespace Engine
