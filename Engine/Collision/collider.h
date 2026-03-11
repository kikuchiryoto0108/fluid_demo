//==============================================================================
//  File   : collider.h
//  Brief  : コライダー基底クラス - 衝突判定の抽象インターフェース
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include <DirectXMath.h>

namespace Engine {
    using namespace DirectX;

    //==========================================================
    // コライダーの形状タイプ
    //==========================================================
    enum class ColliderType {
        BOX,      // ボックス（AABB）
        SPHERE    // 球体
    };

    //==========================================================
    // コライダーの用途タイプ
    //==========================================================
    enum class ColliderPurpose {
        BODY,     // 物理ボディ用
        ATTACK,   // 攻撃判定用
        TRIGGER   // トリガー領域用
    };

    //==========================================================
    // Collider - コライダー基底クラス
    //==========================================================
    class Collider {
    public:
        virtual ~Collider() = default;

        // --- 純粋仮想関数（派生クラスで実装必須） ---
        virtual ColliderType GetType() const = 0;
        virtual bool Intersects(const Collider* other) const = 0;
        virtual XMFLOAT3 GetCenter() const = 0;
        virtual void GetBounds(XMFLOAT3& outMin, XMFLOAT3& outMax) const = 0;

        // --- 用途の取得・設定 ---
        ColliderPurpose GetPurpose() const { return m_purpose; }
        void SetPurpose(ColliderPurpose purpose) { m_purpose = purpose; }

    protected:
        // --- トランスフォーム情報 ---
        XMFLOAT3 m_position = { 0.0f, 0.0f, 0.0f };  // 位置
        XMFLOAT3 m_rotation = { 0.0f, 0.0f, 0.0f };  // 回転
        XMFLOAT3 m_scale = { 1.0f, 1.0f, 1.0f };     // スケール
        ColliderPurpose m_purpose = ColliderPurpose::BODY;
    };

} // namespace Engine
