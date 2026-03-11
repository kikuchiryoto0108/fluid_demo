//==============================================================================
//  File   : box_collider.h
//  Brief  : ボックスコライダー - AABB衝突判定クラス
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include "collider.h"
#include <memory>

namespace Engine {

    class SphereCollider;

    //==========================================================
    // BoxCollider - 軸平行境界ボックス（AABB）コライダー
    //==========================================================
    class BoxCollider : public Collider {
    public:
        BoxCollider();
        BoxCollider(const XMFLOAT3& center, const XMFLOAT3& size);
        ~BoxCollider() override = default;

        // --- コピー・ムーブ操作 ---
        BoxCollider(const BoxCollider& other);
        BoxCollider& operator=(const BoxCollider& other);
        BoxCollider(BoxCollider&& other) noexcept = default;
        BoxCollider& operator=(BoxCollider&& other) noexcept = default;

        // --- Collider基底クラスの実装 ---
        ColliderType GetType() const override { return ColliderType::BOX; }
        bool Intersects(const Collider* other) const override;
        XMFLOAT3 GetCenter() const override;
        void GetBounds(XMFLOAT3& outMin, XMFLOAT3& outMax) const override;

        // --- 設定メソッド ---
        void SetCenter(const XMFLOAT3& center);
        void SetSize(const XMFLOAT3& size);
        void SetTransform(const XMFLOAT3& position, const XMFLOAT3& rotation, const XMFLOAT3& scale);

        // --- 取得メソッド ---
        XMFLOAT3 GetSize() const { return m_size; }
        XMFLOAT3 GetMin() const { return m_worldMin; }
        XMFLOAT3 GetMax() const { return m_worldMax; }

        // --- 衝突判定メソッド ---
        bool Contains(const XMFLOAT3& point) const;
        bool ComputePenetration(const BoxCollider* other, XMFLOAT3& outPenetration) const;

        // --- ファクトリメソッド ---
        static std::unique_ptr<BoxCollider> Create(const XMFLOAT3& center, const XMFLOAT3& size);

    private:
        void UpdateWorldBounds();
        bool IntersectsBox(const BoxCollider* other) const;
        bool IntersectsSphere(const SphereCollider* other) const;

        // --- メンバ変数 ---
        XMFLOAT3 m_size = { 1.0f, 1.0f, 1.0f };           // ローカルサイズ
        XMFLOAT3 m_worldMin = { -0.5f, -0.5f, -0.5f };    // ワールド空間の最小点
        XMFLOAT3 m_worldMax = { 0.5f, 0.5f, 0.5f };       // ワールド空間の最大点
    };

} // namespace Engine
