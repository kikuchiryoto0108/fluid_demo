//==============================================================================
//  File   : bullet.cpp
//  Brief  : 弾丸クラス - 弾丸の移動・衝突・寿命管理
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "bullet.h"
#include "Engine/Core/renderer.h"
#include "Engine/Graphics/primitive.h"
#include "Engine/Collision/collision_system.h"
#include "Engine/Collision/map_collision.h"

namespace Game {

//==========================================================
// コンストラクタ
//==========================================================
Bullet::Bullet()
    : GameObjectBase()  // 基底クラスのコンストラクタを呼び出す
    , velocity(0, 0, 0)
    , lifeTime(0)
    , ownerPlayerId(0) {
    // 初期状態は非アクティブ
    m_active = false;
    // 弾丸のデフォルトサイズを設定
    m_scale = XMFLOAT3(0.2f, 0.2f, 0.2f);
}

//==========================================================
// デストラクタ
// 基底クラスのデストラクタでCollisionSystemから自動解除
//==========================================================
Bullet::~Bullet() {
    // 基底クラスのデストラクタで自動的にUnregisterCollider()が呼ばれる
}

//==========================================================
// ムーブコンストラクタ
//==========================================================
Bullet::Bullet(Bullet&& other) noexcept
    : GameObjectBase(std::move(other))  // 基底クラスのムーブコンストラクタを呼び出す
    , velocity(other.velocity)
    , lifeTime(other.lifeTime)
    , visual(std::move(other.visual))
    , ownerPlayerId(other.ownerPlayerId) {
}

//==========================================================
// ムーブ代入演算子
//==========================================================
Bullet& Bullet::operator=(Bullet&& other) noexcept {
    if (this != &other) {
        // 基底クラスのムーブ代入演算子を呼び出す
        GameObjectBase::operator=(std::move(other));

        velocity = other.velocity;
        lifeTime = other.lifeTime;
        visual = std::move(other.visual);
        ownerPlayerId = other.ownerPlayerId;
    }
    return *this;
}

//==========================================================
// 初期化処理
//==========================================================
void Bullet::Initialize(ID3D11ShaderResourceView* texture, const XMFLOAT3& pos, const XMFLOAT3& dir, int ownerId) {
    m_position = pos;
    velocity = { dir.x * 15.0f, dir.y * 15.0f, dir.z * 15.0f };
    lifeTime = 3.0f;
    m_active = true;
    ownerPlayerId = ownerId;   // 弾を撃ったプレイヤーのIDを記録

    // --- 見た目の初期化 ---
    visual.position = m_position;
    visual.scale = XMFLOAT3(0.2f, 0.2f, 0.2f);
    visual.setMesh(Box, 36, texture);
    visual.setBoxCollider(visual.scale);
    visual.markBufferForUpdate();

    // --- コライダーをセットアップ（基底クラスの機能を使用） ---
    SetupCollider(
        { 0.2f, 0.2f, 0.2f },
        Engine::CollisionLayer::PROJECTILE,
        Engine::CollisionLayer::PLAYER | Engine::CollisionLayer::ENEMY
    );
}

//==========================================================
// 更新処理
//==========================================================
void Bullet::Update(float deltaTime) {
    if (!m_active) return;

    // --- 弾の移動 ---
    m_position.x += velocity.x * deltaTime;
    m_position.y += velocity.y * deltaTime;
    m_position.z += velocity.z * deltaTime;

    m_collider.SetCenter(m_position);
    visual.position = m_position;
    visual.markBufferForUpdate();

    // --- マップとの衝突で弾を消す ---
    XMFLOAT3 pen;
    if (Engine::MapCollision::GetInstance().CheckCollision(&m_collider, pen)) {
        Deactivate();
        return;
    }

    // --- 寿命切れで弾を消す ---
    lifeTime -= deltaTime;
    if (lifeTime <= 0) {
        Deactivate();
    }
}

//==========================================================
// 描画処理
//==========================================================
void Bullet::Draw() {
    if (m_active) visual.draw();
}

//==========================================================
// 非アクティブ化
//==========================================================
void Bullet::Deactivate() {
    if (!m_active) return;
    m_active = false;

    // --- 衝突システムから登録解除（基底クラスの機能を使用） ---
    UnregisterCollider();
}

} // namespace Game
