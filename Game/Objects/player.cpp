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
#include "Game/game_manager.h"

// Engine名前空間のDebugLogを使用
using Engine::DebugLog;

namespace Game {

//==========================================================
// コンストラクタ
//==========================================================
Player::Player()
    : GameObjectBase()  // 基底クラスのコンストラクタを呼び出す
    , velocity(0.0f, 0.0f, 0.0f)
    , hp(MAX_HP)            // 最大HP(10)で初期化
    , isAlive(true)
    , isGrounded(false)
    , mapRef(nullptr)
    , playerId(0)
    , viewMode(ViewMode::THIRD_PERSON)
    , cameraYaw(0.0f)
    , cameraPitch(0.0f) {
    DebugLog("[Player::Player] Constructor called\n");
    // 初期位置・スケールを設定（GameObjectBaseのメンバ）
    m_position = XMFLOAT3(0.0f, 3.0f, 0.0f);
    m_scale = XMFLOAT3(WIDTH, HEIGHT, DEPTH);
    m_collider.SetCenter(m_position);
    m_collider.SetSize(m_scale);
}

//==========================================================
// デストラクタ
// 基底クラスのデストラクタでCollisionSystemから自動解除
//==========================================================
Player::~Player() {
    DebugLog("[Player::~Player] Destructor called for player %d\n", playerId);
    // 基底クラスのデストラクタで自動的にUnregisterCollider()が呼ばれる
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
        m_position = XMFLOAT3(3.0f, 3.0f, 0.0f);
    }

    DebugLog("[Player::Initialize] Setting up visual object\n");
    // --- 見た目用GameObjectの初期化 ---
    visualObject.position = m_position;
    visualObject.scale = m_scale;
    visualObject.rotation = m_rotation;
    visualObject.setMesh(Box, 36, texture);
    visualObject.setBoxCollider(m_scale);
    visualObject.markBufferForUpdate();

    // --- コライダーをセットアップ（基底クラスの機能を使用） ---
    SetupCollider(
        m_scale,
        Engine::CollisionLayer::PLAYER,
        Engine::CollisionLayer::PROJECTILE | Engine::CollisionLayer::ENEMY
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

    // --- 水鉄砲の初期化 ---
    m_waterGun = std::make_unique<WaterGun>();
    auto* fluid = GameManager::Instance().GetFluid();
    if (fluid) {
        m_waterGun->Initialize(fluid);
        DebugLog("[Player::Initialize] WaterGun initialized\n");
    } else {
        // 後で初期化される（Update内で）
        DebugLog("[Player::Initialize] WaterGun created, will initialize later\n");
    }
}

//==========================================================
// 位置設定（GameObjectBaseのオーバーライド）
// 見た目オブジェクトの位置も同期する
//==========================================================
void Player::SetPosition(const XMFLOAT3& pos) {
    // 基底クラスのSetPositionを呼び出す（コライダーも同期される）
    GameObjectBase::SetPosition(pos);
    // 見た目オブジェクトの更新
    UpdateVisualObject();
}

//==========================================================
// 回転設定（GameObjectBaseのオーバーライド）
//==========================================================
void Player::SetRotation(const XMFLOAT3& rot) {
    GameObjectBase::SetRotation(rot);
    visualObject.rotation = m_rotation;
    visualObject.markBufferForUpdate();
}

//==========================================================
// 見た目オブジェクトの更新
//==========================================================
void Player::UpdateVisualObject() {
    visualObject.position = m_position;
    visualObject.scale = m_scale;
    visualObject.setBoxCollider(m_scale);
    visualObject.markBufferForUpdate();
}

//==========================================================
// 更新処理
//==========================================================
void Player::Update(float deltaTime) {
    if (!isAlive) return;

    // ★水鉄砲の遅延初期化（fluidがなければ取得を試みる）
    if (m_waterGun && !m_waterGun->IsInitialized()) {
        auto* fluid = GameManager::Instance().GetFluid();
        if (fluid) {
            m_waterGun->Initialize(fluid);
            DebugLog("[Player::Update] WaterGun initialized (delayed)\n");
        }
    }

    // 前フレームの接地状態を保存
    m_wasGrounded = isGrounded;

    // --- 重力適用（接地中は下向き速度を0に保つ） ---
    if (!isGrounded) {
        velocity.y += GRAVITY * deltaTime;
        // 落下速度制限
        if (velocity.y < -30.0f) {
            velocity.y = -30.0f;
        }
    } else {
        // 接地中で下に落ちようとしている場合のみリセット
        if (velocity.y < 0.0f) {
            velocity.y = 0.0f;
        }
    }

    // --- 速度に基づいて位置を更新 ---
    XMFLOAT3 newPosition = m_position;
    newPosition.x += velocity.x * deltaTime;
    newPosition.y += velocity.y * deltaTime;
    newPosition.z += velocity.z * deltaTime;

    // --- コライダーを新しい位置に更新 ---
    m_collider.SetCenter(newPosition);

    // --- マップとの衝突判定 ---
    bool groundedThisFrame = false;
    auto penetrations = Engine::MapCollision::GetInstance().CheckCollisionAll(&m_collider, 2.0f);

    // Y軸の補正を先に処理（地面判定を優先）
    float totalPenY = 0.0f;
    for (const auto& pen : penetrations) {
        if (pen.y > 0.0f) {
            // 上に押し戻し = 地面に着地
            totalPenY = std::max(totalPenY, pen.y);
            groundedThisFrame = true;
        } else if (pen.y < 0.0f) {
            // 下に押し戻し = 天井にぶつかった
            totalPenY = std::min(totalPenY, pen.y);
        }
    }

    // Y軸補正を適用
    if (totalPenY != 0.0f) {
        newPosition.y += totalPenY;
        velocity.y = 0.0f;
    }

    // X, Z軸の補正
    for (const auto& pen : penetrations) {
        if (pen.x != 0.0f) {
            newPosition.x += pen.x;
            velocity.x = 0.0f;
        }
        if (pen.z != 0.0f) {
            newPosition.z += pen.z;
            velocity.z = 0.0f;
        }
    }

    // --- 接地状態の安定化（コヨーテタイム） ---
    if (groundedThisFrame) {
        isGrounded = true;
        m_groundedTimer = m_coyoteTime;
    } else {
        // 猶予時間内は接地状態を維持
        m_groundedTimer -= deltaTime;
        if (m_groundedTimer <= 0.0f) {
            isGrounded = false;
            m_groundedTimer = 0.0f;
        }
    }

    // --- 地面スナップ（接地していたのに今フレーム接地していない場合） ---
    if (m_wasGrounded && !groundedThisFrame && velocity.y <= 0.0f) {
        // 少し下を調べて地面があればスナップ
        XMFLOAT3 snapTestPos = newPosition;
        snapTestPos.y -= 0.15f;  // スナップ距離
        m_collider.SetCenter(snapTestPos);

        auto snapPenetrations = Engine::MapCollision::GetInstance().CheckCollisionAll(&m_collider, 1.0f);
        for (const auto& pen : snapPenetrations) {
            if (pen.y > 0.0f) {
                // 地面が見つかった
                newPosition.y = snapTestPos.y + pen.y;
                velocity.y = 0.0f;
                isGrounded = true;
                m_groundedTimer = m_coyoteTime;
                break;
            }
        }
    }

    // --- 位置確定 ---
    m_position = newPosition;
    m_collider.SetCenter(m_position);

    // --- 摩擦で水平速度を減衰 ---
    if (isGrounded) {
        velocity.x *= FRICTION;
        velocity.z *= FRICTION;
    } else {
        // 空中でも少し減衰（空気抵抗）
        velocity.x *= 0.98f;
        velocity.z *= 0.98f;
    }

    // --- 微小な速度はゼロに切り捨て ---
    if (fabsf(velocity.x) < 0.01f) velocity.x = 0.0f;
    if (fabsf(velocity.z) < 0.01f) velocity.z = 0.0f;

    // --- 見た目を位置・回転に同期 ---
    visualObject.position = m_position;
    visualObject.rotation = m_rotation;
    visualObject.markBufferForUpdate();

    // --- 銃の更新 ---
    //if (m_gun) {
    //    m_gun->Update(this, deltaTime);
    //}

    // --- 水鉄砲の更新 ---
    if (m_waterGun) {
        m_waterGun->Update(this, deltaTime);
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
    // 接地中またはコヨーテタイム内ならジャンプ可能
    if (isGrounded || m_groundedTimer > 0.0f) {
        velocity.y = JUMP_POWER;
        isGrounded = false;
        m_groundedTimer = 0.0f;  // ジャンプしたら猶予をリセット
    }
}

//==========================================================
// めり込み解消処理
//==========================================================
void Player::ApplyPenetration(const XMFLOAT3& penetration) {
    // めり込み分だけ位置を押し戻す
    m_position.x += penetration.x;
    m_position.y += penetration.y;
    m_position.z += penetration.z;

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

    SyncCollider();
    UpdateVisualObject();
}

//==========================================================
// 描画処理
//==========================================================
void Player::Draw() {
    if (!isAlive) {
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
    m_position = pos;
    // マップがあれば地面にめり込まないよう補正
    if (mapRef) {
        float groundY = mapRef->GetGroundHeight(m_position.x, m_position.z);
        if (m_position.y < groundY + m_scale.y * 0.5f) {
            m_position.y = groundY + m_scale.y * 0.5f;
        }
    }
    SyncCollider();
    UpdateVisualObject();
}

//==========================================================
// 強制回転設定（ネットワーク用）
//==========================================================
void Player::ForceSetRotation(const XMFLOAT3& rot) {
    m_rotation = rot;
    visualObject.rotation = m_rotation;
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
    m_position = spawnPoint;
    hp = MAX_HP;       // 最大HPで復活
    isAlive = true;
    velocity = { 0.0f, 0.0f, 0.0f };
    isGrounded = false;
    SyncCollider();
    UpdateVisualObject();
}

//==========================================================
// 強制死亡（ネットワーク同期用）
//==========================================================
void Player::ForceDeath() {
    hp = 0;
    isAlive = false;
}

} // namespace Game
