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
    : position(0, 0, 0)
    , velocity(0, 0, 0)
    , lifeTime(0)
    , active(false)
    , collider({ 0, 0, 0 }, { 0.2f, 0.2f, 0.2f })
    , m_collisionId(0)
    , ownerPlayerId(0) {
}

//==========================================================
// デストラクタ
//==========================================================
Bullet::~Bullet() {
    if (m_collisionId != 0) {
        Engine::CollisionSystem::GetInstance().Unregister(m_collisionId);
        m_collisionId = 0;
    }
}

//==========================================================
// ムーブコンストラクタ
//==========================================================
Bullet::Bullet(Bullet&& other) noexcept
    : position(other.position)
    , velocity(other.velocity)
    , lifeTime(other.lifeTime)
    , active(other.active)
    , collider(std::move(other.collider))
    , visual(std::move(other.visual))
    , m_collisionId(other.m_collisionId)
    , ownerPlayerId(other.ownerPlayerId) {
    other.m_collisionId = 0;
}

//==========================================================
// ムーブ代入演算子
//==========================================================
Bullet& Bullet::operator=(Bullet&& other) noexcept {
    if (this != &other) {
        if (m_collisionId != 0) {
            Engine::CollisionSystem::GetInstance().Unregister(m_collisionId);
        }

        position = other.position;
        velocity = other.velocity;
        lifeTime = other.lifeTime;
        active = other.active;
        collider = std::move(other.collider);
        visual = std::move(other.visual);
        m_collisionId = other.m_collisionId;
        ownerPlayerId = other.ownerPlayerId;

        other.m_collisionId = 0;
    }
    return *this;
}

//==========================================================
// 初期化処理
//==========================================================
void Bullet::Initialize(ID3D11ShaderResourceView* texture, const XMFLOAT3& pos, const XMFLOAT3& dir, int ownerId) {
    position = pos;
    velocity = { dir.x * 15.0f, dir.y * 15.0f, dir.z * 15.0f };
    lifeTime = 3.0f;
    active = true;
    ownerPlayerId = ownerId;   // 弾を撃ったプレイヤーのIDを記録

    // --- 見た目の初期化 ---
    visual.position = position;
    visual.scale = XMFLOAT3(0.2f, 0.2f, 0.2f);
    visual.setMesh(Box, 36, texture);
    visual.setBoxCollider(visual.scale);
    visual.markBufferForUpdate();

    // --- 衝突判定の初期化 ---
    collider.SetCenter(position);
    collider.SetSize({ 0.2f, 0.2f, 0.2f });

    m_collisionId = Engine::CollisionSystem::GetInstance().Register(
        &collider,
        Engine::CollisionLayer::PROJECTILE,
        Engine::CollisionLayer::PLAYER | Engine::CollisionLayer::ENEMY,
        this
    );
}

//==========================================================
// 更新処理
//==========================================================
void Bullet::Update(float deltaTime) {
    if (!active) return;

    // --- 弾の移動 ---
    position.x += velocity.x * deltaTime;
    position.y += velocity.y * deltaTime;
    position.z += velocity.z * deltaTime;

    collider.SetCenter(position);
    visual.position = position;
    visual.markBufferForUpdate();

    // --- マップとの衝突で弾を消す ---
    XMFLOAT3 pen;
    if (Engine::MapCollision::GetInstance().CheckCollision(&collider, pen)) {
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
    if (active) visual.draw();
}

//==========================================================
// 非アクティブ化
//==========================================================
void Bullet::Deactivate() {
    if (!active) return;
    active = false;

    // --- 衝突システムから登録解除 ---
    if (m_collisionId != 0) {
        Engine::CollisionSystem::GetInstance().Unregister(m_collisionId);
        m_collisionId = 0;
    }
}

} // namespace Game
