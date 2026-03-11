//==============================================================================
//  File   : player.cpp
//  Brief  : プレイヤークラス - 移動・ジャンプ・衝突・HP管理
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "player.h"
#include "Game/Map/map.h"
#include "Engine/Graphics/primitive.h"
#include "Engine/Collision/collision_system.h"
#include "Engine/Collision/map_collision.h"
#include "Engine/Core/debug_log.h"
#include <cmath>
#include <algorithm>

// Engine名前空間のDebugLogを使用
using Engine::DebugLog;

namespace Game {

//==========================================================
// コンストラクタ
//==========================================================
Player::Player()
    : position(0.0f, 3.0f, 0.0f)
    , velocity(0.0f, 0.0f, 0.0f)
    , rotation(0.0f, 0.0f, 0.0f)
    , scale(WIDTH, HEIGHT, DEPTH)
    , m_collisionId(0)
    , hp(MAX_HP)            // 最大HP(10)で初期化
    , isAlive(true)
    , isGrounded(false)
    , mapRef(nullptr)
    , playerId(0)
    , viewMode(ViewMode::THIRD_PERSON)
    , cameraYaw(0.0f)
    , cameraPitch(0.0f) {
    DebugLog("[Player::Player] Constructor called\n");
    collider.SetCenter(position);
    collider.SetSize(scale);
}

//==========================================================
// デストラクタ
//==========================================================
Player::~Player() {
    DebugLog("[Player::~Player] Destructor called for player %d\n", playerId);
    if (m_collisionId != 0) {
        Engine::CollisionSystem::GetInstance().Unregister(m_collisionId);
        m_collisionId = 0;
    }
}

//==========================================================
// 初期化処理
//==========================================================
void Player::Initialize(Map* map, ID3D11ShaderResourceView* texture, int id, ViewMode mode) {
    DebugLog("[Player::Initialize] START: playerId=%d, viewMode=%d\n", id, (int)mode);
    
    mapRef = map;
    playerId = id;
    viewMode = mode;

    // Player2は少しずらした位置に配置
    if (playerId == 2) {
        position = XMFLOAT3(3.0f, 3.0f, 0.0f);
    }

    DebugLog("[Player::Initialize] Setting up visual object\n");
    // --- 見た目用GameObjectの初期化 ---
    visualObject.position = position;
    visualObject.scale = scale;
    visualObject.rotation = rotation;
    visualObject.setMesh(Box, 36, texture);
    visualObject.setBoxCollider(scale);
    visualObject.markBufferForUpdate();

    // --- 衝突判定用コライダーの初期化 ---
    collider.SetCenter(position);
    collider.SetSize(scale);

    // --- 衝突システムに登録（弾と敵に反応する） ---
    m_collisionId = Engine::CollisionSystem::GetInstance().Register(
        &collider,
        Engine::CollisionLayer::PLAYER,
        Engine::CollisionLayer::PROJECTILE | Engine::CollisionLayer::ENEMY,
        this
    );
    DebugLog("[Player::Initialize] Collision registered: id=%d\n", m_collisionId);

    // --- 銃の初期化（テクスチャは自動的にモデルから読み込まれる） ---
    DebugLog("[Player::Initialize] Creating gun object\n");
    m_gun = std::make_unique<Gun>();
    DebugLog("[Player::Initialize] Loading gun model: resource/model/M1911.FBX\n");
    if (m_gun->LoadFromFile("resource/model/M1911.FBX")) {
        DebugLog("[Player::Initialize] Gun loaded successfully, setting offset and scale\n");
        // 銃のオフセット設定（右前方、目の高さ）
        m_gun->SetOffset(XMFLOAT3(1.5f, 0.9f, 0.5f));
        // 銃のスケール調整（モデルサイズに応じて調整）
        m_gun->SetScale(XMFLOAT3(2.0f, 2.0f, 2.0f));
    } else {
        DebugLog("[Player::Initialize] FAILED to load gun model!\n");
    }
    
    DebugLog("[Player::Initialize] COMPLETE for player %d\n", playerId);
}

//==========================================================
// 位置設定
//==========================================================
void Player::SetPosition(const XMFLOAT3& pos) {
    position = pos;
    UpdateCollider();
}

//==========================================================
// コライダー更新
//==========================================================
void Player::UpdateCollider() {
    collider.SetCenter(position);
    collider.SetSize(scale);

    visualObject.position = position;
    visualObject.scale = scale;
    visualObject.setBoxCollider(scale);
    visualObject.markBufferForUpdate();
}

//==========================================================
// 更新処理
//==========================================================
void Player::Update(float deltaTime) {
    if (!isAlive) return;

    // --- 空中なら重力を適用 ---
    if (!isGrounded) {
        velocity.y += GRAVITY * deltaTime;
    }

    // --- 速度に基づいて位置を更新 ---
    position.x += velocity.x * deltaTime;
    position.y += velocity.y * deltaTime;
    position.z += velocity.z * deltaTime;

    UpdateCollider();

    // --- マップとの衝突判定（グリッドベース） ---
    isGrounded = false;
    auto penetrations = Engine::MapCollision::GetInstance().CheckCollisionAll(&collider, 3.0f);
    for (const auto& pen : penetrations) {
        ApplyPenetration(pen);
    }

    // --- 摩擦で水平速度を減衰 ---
    velocity.x *= FRICTION;
    velocity.z *= FRICTION;

    // --- 微小な速度はゼロに切り捨て ---
    if (fabsf(velocity.x) < 0.01f) velocity.x = 0.0f;
    if (fabsf(velocity.z) < 0.01f) velocity.z = 0.0f;

    // --- 見た目を位置・回転に同期 ---
    visualObject.position = position;
    visualObject.rotation = rotation;
    visualObject.markBufferForUpdate();

    // --- 銃の更新 ---
    if (m_gun) {
        m_gun->Update(this, deltaTime);
    }
}

//==========================================================
// 移動処理
//==========================================================
void Player::Move(const XMFLOAT3& direction, float deltaTime) {
    velocity.x = direction.x * MOVE_SPEED;
    velocity.z = direction.z * MOVE_SPEED;
}

//==========================================================
// ジャンプ処理
//==========================================================
void Player::Jump() {
    // 接地中のみジャンプ可能
    if (isGrounded) {
        velocity.y = JUMP_POWER;
        isGrounded = false;
    }
}

//==========================================================
// めり込み解消処理
//==========================================================
void Player::ApplyPenetration(const XMFLOAT3& penetration) {
    // めり込み分だけ位置を押し戻す
    position.x += penetration.x;
    position.y += penetration.y;
    position.z += penetration.z;

    // 押し戻した軸の速度をゼロにする
    if (penetration.x != 0.0f) velocity.x = 0.0f;
    if (penetration.y != 0.0f) {
        velocity.y = 0.0f;
        // 上方向に押し戻された = 地面に着地した
        if (penetration.y > 0.0f) {
            isGrounded = true;
        }
    }
    if (penetration.z != 0.0f) velocity.z = 0.0f;

    UpdateCollider();
}

//==========================================================
// 描画処理
//==========================================================
void Player::Draw() {
    if (!isAlive) {
        OutputDebugStringA("[Player::Draw] SKIP: not alive\n");
        return;
    }

    char buf[256];
    sprintf_s(buf, "[Player::Draw] id=%d pos=(%.1f,%.1f,%.1f) scale=(%.1f,%.1f,%.1f)\n",
        playerId,
        visualObject.position.x, visualObject.position.y, visualObject.position.z,
        visualObject.scale.x, visualObject.scale.y, visualObject.scale.z);

    static int frameCount = 0;
    if (frameCount++ % 60 == 0) {  // 1秒に1回だけ出力
        OutputDebugStringA(buf);
    }

    visualObject.draw();
    if (m_gun) {
        m_gun->Draw();
    }
}

//==========================================================
// GameObjectアクセス
//==========================================================
GameObject* Player::GetGameObject() {
    return &visualObject;
}

//==========================================================
// 強制位置設定（ネットワーク用）
//==========================================================
void Player::ForceSetPosition(const XMFLOAT3& pos) {
    position = pos;
    // マップがあれば地面にめり込まないよう補正
    if (mapRef) {
        float groundY = mapRef->GetGroundHeight(position.x, position.z);
        if (position.y < groundY + scale.y * 0.5f) {
            position.y = groundY + scale.y * 0.5f;
        }
    }
    UpdateCollider();
}

//==========================================================
// 強制回転設定（ネットワーク用）
//==========================================================
void Player::ForceSetRotation(const XMFLOAT3& rot) {
    rotation = rot;
    visualObject.rotation = rotation;
    visualObject.markBufferForUpdate();
}

//==========================================================
// カメラ角度設定・取得
//==========================================================
void Player::SetCameraAngles(float yaw, float pitch) {
    cameraYaw = yaw;
    cameraPitch = pitch;
}

float Player::GetCameraYaw() const { return cameraYaw; }
float Player::GetCameraPitch() const { return cameraPitch; }

//==========================================================
// ダメージ処理
//==========================================================
void Player::TakeDamage(int dmg) {
    hp -= dmg;
    if (hp <= 0) {
        hp = 0;
        isAlive = false;
    }
}

//==========================================================
// リスポーン処理
//==========================================================
void Player::Respawn(const XMFLOAT3& spawnPoint) {
    position = spawnPoint;
    hp = MAX_HP;       // 最大HPで復活
    isAlive = true;
    velocity = { 0.0f, 0.0f, 0.0f };
    isGrounded = false;
    UpdateCollider();
}

//==========================================================
// 強制死亡（ネットワーク同期用）
//==========================================================
void Player::ForceDeath() {
    hp = 0;
    isAlive = false;
}

} // namespace Game
