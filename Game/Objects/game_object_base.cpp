//==============================================================================
//  File   : game_object_base.cpp
//  Brief  : ゲームオブジェクト基底クラス - トランスフォームとコライダーの自動同期
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "game_object_base.h"
#include "Engine/Collision/collision_system.h"

namespace Game {

//==========================================================
// コンストラクタ
//==========================================================
GameObjectBase::GameObjectBase()
    : m_position(0.0f, 0.0f, 0.0f)
    , m_rotation(0.0f, 0.0f, 0.0f)
    , m_scale(1.0f, 1.0f, 1.0f)
    , m_collider({ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f })
    , m_collisionId(0)
    , m_colliderEnabled(true)
    , m_active(true) {
}

//==========================================================
// デストラクタ
// CollisionSystemから自動的に登録解除
//==========================================================
GameObjectBase::~GameObjectBase() {
    UnregisterCollider();
}

//==========================================================
// ムーブコンストラクタ
//==========================================================
GameObjectBase::GameObjectBase(GameObjectBase&& other) noexcept
    : m_position(other.m_position)
    , m_rotation(other.m_rotation)
    , m_scale(other.m_scale)
    , m_collider(std::move(other.m_collider))
    , m_collisionId(other.m_collisionId)
    , m_colliderEnabled(other.m_colliderEnabled)
    , m_active(other.m_active) {
    // 元オブジェクトのIDをクリア（二重解除防止）
    other.m_collisionId = 0;
}

//==========================================================
// ムーブ代入演算子
//==========================================================
GameObjectBase& GameObjectBase::operator=(GameObjectBase&& other) noexcept {
    if (this != &other) {
        // 既存のコライダーを解除
        UnregisterCollider();

        m_position = other.m_position;
        m_rotation = other.m_rotation;
        m_scale = other.m_scale;
        m_collider = std::move(other.m_collider);
        m_collisionId = other.m_collisionId;
        m_colliderEnabled = other.m_colliderEnabled;
        m_active = other.m_active;

        // 元オブジェクトのIDをクリア（二重解除防止）
        other.m_collisionId = 0;
    }
    return *this;
}

//==========================================================
// 位置設定
// コライダーの位置も自動同期
//==========================================================
void GameObjectBase::SetPosition(const XMFLOAT3& pos) {
    m_position = pos;
    SyncCollider();
}

//==========================================================
// 回転設定
//==========================================================
void GameObjectBase::SetRotation(const XMFLOAT3& rot) {
    m_rotation = rot;
    // 回転はAABBコライダーには影響しないが、派生クラスで使用する可能性あり
}

//==========================================================
// スケール設定
// コライダーのサイズも自動同期
//==========================================================
void GameObjectBase::SetScale(const XMFLOAT3& scale) {
    m_scale = scale;
    SyncCollider();
}

//==========================================================
// コライダーの位置・サイズを同期
//==========================================================
void GameObjectBase::SyncCollider() {
    m_collider.SetCenter(m_position);
    m_collider.SetSize(m_scale);
}

//==========================================================
// CollisionSystemへの登録
//==========================================================
bool GameObjectBase::SetupCollider(const XMFLOAT3& size, Engine::CollisionLayer layer, Engine::CollisionLayer mask) {
    // 既存の登録があれば解除
    if (m_collisionId != 0) {
        UnregisterCollider();
    }

    // コライダーを初期化
    m_collider.SetCenter(m_position);
    m_collider.SetSize(size);

    // CollisionSystemに登録
    m_collisionId = Engine::CollisionSystem::GetInstance().Register(
        &m_collider,
        layer,
        mask,
        this  // userDataとしてthisポインタを渡す
    );

    return m_collisionId != 0;
}

//==========================================================
// コライダーの有効/無効切り替え
//==========================================================
void GameObjectBase::EnableCollider(bool enabled) {
    m_colliderEnabled = enabled;
    if (m_collisionId != 0) {
        Engine::CollisionSystem::GetInstance().SetEnabled(m_collisionId, enabled);
    }
}

//==========================================================
// CollisionSystemから登録解除
//==========================================================
void GameObjectBase::UnregisterCollider() {
    if (m_collisionId != 0) {
        Engine::CollisionSystem::GetInstance().Unregister(m_collisionId);
        m_collisionId = 0;
    }
}

//==========================================================
// アクティブ状態設定
//==========================================================
void GameObjectBase::SetActive(bool active) {
    m_active = active;
    // アクティブ状態に応じてコライダーも有効/無効化
    EnableCollider(active && m_colliderEnabled);
}

} // namespace Game
