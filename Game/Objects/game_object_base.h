//==============================================================================
//  File   : game_object_base.h
//  Brief  : ゲームオブジェクト基底クラス - コライダー管理を含む共通機能
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include <DirectXMath.h>
#include <memory>
#include <cstdint>
#include "Engine/Collision/box_collider.h"
#include "Engine/Collision/collision_system.h"

namespace Game {

using namespace DirectX;

//==========================================================
// GameObjectBase - ゲームオブジェクト基底クラス
// 
// トランスフォーム管理とコライダーの自動同期を提供する。
// Player, Bullet, Block等のゲームオブジェクトはこのクラスを継承する。
//==========================================================
class GameObjectBase {
public:
    // --- コンストラクタ・デストラクタ ---
    GameObjectBase();
    virtual ~GameObjectBase();

    // --- コピー禁止・ムーブ許可 ---
    GameObjectBase(const GameObjectBase&) = delete;
    GameObjectBase& operator=(const GameObjectBase&) = delete;
    GameObjectBase(GameObjectBase&& other) noexcept;
    GameObjectBase& operator=(GameObjectBase&& other) noexcept;

    //==========================================================
    // 純粋仮想関数（派生クラスで実装必須）
    //==========================================================
    virtual void Update(float deltaTime) = 0;
    virtual void Draw() = 0;

    //==========================================================
    // トランスフォーム管理
    //==========================================================
    // --- 位置 ---
    const XMFLOAT3& GetPosition() const { return m_position; }
    virtual void SetPosition(const XMFLOAT3& pos);

    // --- 回転 ---
    const XMFLOAT3& GetRotation() const { return m_rotation; }
    virtual void SetRotation(const XMFLOAT3& rot);

    // --- スケール ---
    const XMFLOAT3& GetScale() const { return m_scale; }
    virtual void SetScale(const XMFLOAT3& scale);

    //==========================================================
    // コライダー管理
    //==========================================================
    // --- CollisionSystemへの登録 ---
    // size: コライダーのサイズ
    // layer: 所属レイヤー
    // mask: 衝突対象マスク
    // 戻り値: 登録成功でtrue
    bool SetupCollider(const XMFLOAT3& size, Engine::CollisionLayer layer, Engine::CollisionLayer mask);

    // --- コライダーの有効/無効切り替え ---
    void EnableCollider(bool enabled);

    // --- コライダーへのポインタ取得 ---
    Engine::BoxCollider* GetCollider() { return &m_collider; }
    const Engine::BoxCollider* GetCollider() const { return &m_collider; }

    // --- 衝突ID取得 ---
    uint32_t GetCollisionId() const { return m_collisionId; }

    // --- コライダーが登録済みかどうか ---
    bool HasCollider() const { return m_collisionId != 0; }

    //==========================================================
    // アクティブ状態管理
    //==========================================================
    bool IsActive() const { return m_active; }
    virtual void SetActive(bool active);

protected:
    // --- コライダーの位置・サイズを同期（派生クラスでオーバーライド可能） ---
    virtual void SyncCollider();

    // --- CollisionSystemから登録解除 ---
    void UnregisterCollider();

    //==========================================================
    // メンバ変数
    //==========================================================
    // --- トランスフォーム ---
    XMFLOAT3 m_position = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 m_rotation = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 m_scale = { 1.0f, 1.0f, 1.0f };

    // --- コライダー ---
    Engine::BoxCollider m_collider;
    uint32_t m_collisionId = 0;
    bool m_colliderEnabled = true;

    // --- アクティブ状態 ---
    bool m_active = true;
};

} // namespace Game
