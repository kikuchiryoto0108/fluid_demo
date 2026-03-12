//==============================================================================
//  File   : destructible_wall.h
//  Brief  : 破壊可能な壁 - 弾が当たると破片が落下する
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include <DirectXMath.h>
#include <vector>
#include <memory>
#include <string>
#include "Engine/Graphics/mesh.h"
#include "Engine/Graphics/model_data.h"
#include "Engine/Collision/box_collider.h"

namespace Game {

    using namespace DirectX;

    //==========================================================
    // 破片の物理状態
    //==========================================================
    enum class FragmentState {
        FROZEN,     // 固定（壁の一部として静止）
        FALLING,    // 落下中（物理シミュレーション有効）
        RESTING,    // 地面で静止
        DESTROYED   // 破壊済み（非表示）
    };

    //==========================================================
    // 壁の破片
    //==========================================================
    struct WallFragment {
        // 位置・物理
        XMFLOAT3 position = { 0, 0, 0 };
        XMFLOAT3 initialPosition = { 0, 0, 0 };
        XMFLOAT3 velocity = { 0, 0, 0 };
        XMFLOAT3 rotation = { 0, 0, 0 };
        XMFLOAT3 initialRotation = { 0, 0, 0 };
        XMFLOAT3 angularVelocity = { 0, 0, 0 };
        XMFLOAT3 scale = { 1, 1, 1 };

        // バウンディング
        XMFLOAT3 boundsMin = { 0, 0, 0 };
        XMFLOAT3 boundsMax = { 0, 0, 0 };
        XMFLOAT3 center = { 0, 0, 0 };  // メッシュ中心

        // 状態
        FragmentState state = FragmentState::FROZEN;
        float lifetime = 0.0f;

        // 描画
        std::shared_ptr<Engine::Mesh> mesh;
        ID3D11ShaderResourceView* texture = nullptr;

        // 衝突
        Engine::BoxCollider collider;

        static constexpr float MAX_LIFETIME = 15.0f;
    };

    //==========================================================
    // 破壊可能な壁クラス
    //==========================================================
    class DestructibleWall {
    public:
        DestructibleWall();
        ~DestructibleWall();

        // --- ファイルから読み込み ---
        bool LoadFromFile(const std::string& filepath, ID3D11Device* pDevice);

        // --- 位置・スケール設定 ---
        void SetPosition(const XMFLOAT3& pos);
        void SetRotation(const XMFLOAT3& rot);
        void SetScale(const XMFLOAT3& scale);

        // --- 更新・描画 ---
        void Update(float deltaTime);
        void Draw();

        // --- 破壊 ---
        // hitPointを中心にradius内の破片を落下させる
        // 戻り値: 落下開始した破片数
        int BreakAtPoint(const XMFLOAT3& hitPoint, float radius);

        // --- 弾との衝突判定 ---
        // 衝突した破片があればtrue、hitPointに衝突位置を返す
        bool CheckBulletHit(const XMFLOAT3& bulletPos, float bulletRadius, XMFLOAT3& outHitPoint);

        // --- リセット ---
        void Reset();

        // --- 状態取得 ---
        size_t GetFragmentCount() const { return m_fragments.size(); }
        size_t GetFrozenCount() const;
        size_t GetFallingCount() const;
        bool IsFullyDestroyed() const;

        void SetGroundY(float y) { m_groundY = y; }

    private:
        float m_groundY = 0.0f;  // GROUND_Yの代わり

        void UpdateFragment(WallFragment& frag, float deltaTime);
        void CalculateFragmentBounds(WallFragment& frag);
        float DistanceSquared(const XMFLOAT3& a, const XMFLOAT3& b) const;
        XMFLOAT3 TransformPoint(const XMFLOAT3& local) const;

    private:
        std::vector<WallFragment> m_fragments;
        Engine::ModelData m_modelData;

        // 壁全体のトランスフォーム
        XMFLOAT3 m_position = { 0, 0, 0 };
        XMFLOAT3 m_rotation = { 0, 0, 0 };
        XMFLOAT3 m_scale = { 1, 1, 1 };

        // 物理定数
        static constexpr float GRAVITY = -15.0f;
        static constexpr float GROUND_Y = 0.0f;
        static constexpr float BOUNCE_FACTOR = 0.3f;
        static constexpr float FRICTION = 0.7f;
    };

} // namespace Game
