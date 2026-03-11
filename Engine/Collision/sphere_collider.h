//==============================================================================
//  File   : sphere_collider.h
//  Brief  : 球体コライダー - 球形衝突判定クラス
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include "collider.h"

namespace Engine {

    //==========================================================
    // SphereCollider - 球体コライダークラス
    //==========================================================
    class SphereCollider : public Collider {
    public:
        SphereCollider();
        SphereCollider(const XMFLOAT3& center, float radius);
        ~SphereCollider() override = default;

        // --- Collider基底クラスの実装 ---
        ColliderType GetType() const override { return ColliderType::SPHERE; }
        bool Intersects(const Collider* other) const override;
        XMFLOAT3 GetCenter() const override;
        void GetBounds(XMFLOAT3& outMin, XMFLOAT3& outMax) const override;

        // --- 設定メソッド ---
        void SetCenter(const XMFLOAT3& center);
        void SetRadius(float radius);

        // --- 取得メソッド ---
        float GetRadius() const { return m_radius; }
        float GetWorldRadius() const { return m_worldRadius; }

    private:
        void UpdateWorldRadius();

        // --- メンバ変数 ---
        float m_radius = 1.0f;       // ローカル半径
        float m_worldRadius = 1.0f;  // ワールド空間での半径
    };

} // namespace Engine
