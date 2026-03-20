//==============================================================================
//  File   : player.h
//  Brief  : プレイヤークラス - 操作可能なキャラクターの制御
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/11
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once
#include "main.h"
#include "Game/Objects/game_object.h"
#include "Game/Objects/game_object_base.h"
#include "Game/Objects/gun.h"
#include "Engine/Collision/box_collider.h"
#include "Game/Objects/water_gun.h"
#include <DirectXMath.h>
#include <memory>

using namespace DirectX;

namespace Game {

    class Map;

//==========================================================
// 視点モード列挙型
//==========================================================
enum class ViewMode {
    THIRD_PERSON,   // 三人称視点（TPS）
    FIRST_PERSON    // 一人称視点（FPS）
};

//==========================================================
// プレイヤークラス
// GameObjectBaseを継承してコライダー管理を共通化
//==========================================================
class Player : public GameObjectBase {
private:
    // --- 速度（位置・回転・スケールはGameObjectBaseから継承） ---
    XMFLOAT3 velocity;

    // --- 接地安定化 ---
    bool m_wasGrounded = false;      // 前フレームの接地状態
    float m_groundedTimer = 0.0f;    // 接地猶予タイマー（コヨーテタイム）
    float m_coyoteTime = 0.1f;       // 接地猶予時間（秒

    // --- 衝突判定（コライダーはGameObjectBaseから継承） ---
    GameObject visualObject;

    // --- 銃オブジェクト ---
    std::unique_ptr<Gun> m_gun;

    // --- HP・生存状態 ---
    int hp;
    bool isAlive;

    // --- Playerの大きさ ---
    static constexpr float WIDTH = 0.8f;
    static constexpr float HEIGHT = 1.8f;
    static constexpr float DEPTH = 0.8f;

    // --- Player定数 ---
    static constexpr float GRAVITY = -9.8f;
    static constexpr float JUMP_POWER = 5.0f;
    static constexpr float MOVE_SPEED = 5.0f;
    static constexpr float FRICTION = 0.85f;

    // --- HPと被ダメージ ---
    static constexpr int MAX_HP = 10;       // 最大HP
    static constexpr int BULLET_DMG = 1;    // 銃から受けるダメージ

    // --- 接地・マップ参照 ---
    bool isGrounded;
    Map* mapRef;
    int playerId;
    ViewMode viewMode;

    // --- カメラ角度 ---
    float cameraYaw;
    float cameraPitch;

    // --- 内部ヘルパー関数 ---
    void UpdateVisualObject();

    // 水鉄砲
    std::unique_ptr<WaterGun> m_waterGun;

public:
    Player();
    ~Player();

    // --- 初期化・更新・描画（Update/DrawはGameObjectBaseからオーバーライド） ---
    void Initialize(Map* map, ID3D11ShaderResourceView* texture, int id = 0, ViewMode mode = ViewMode::THIRD_PERSON);
    void Update(float deltaTime) override;
    void Draw() override;

    // --- 移動・ジャンプ ---
    void Move(const XMFLOAT3& direction, float deltaTime);
    void Jump();

    // --- ゲッター ---
    // 既存インターフェースとの互換性を維持
    XMFLOAT3 GetPosition() const { return m_position; }
    XMFLOAT3 GetRotation() const { return m_rotation; }
    const Engine::BoxCollider& GetCollider() const { return m_collider; }
    Engine::BoxCollider* GetColliderPtr() { return &m_collider; }
    bool IsGrounded() const { return isGrounded; }
    int GetPlayerId() const { return playerId; }
    ViewMode GetViewMode() const { return viewMode; }
    uint32_t GetCollisionId() const { return m_collisionId; }

    // --- セッター（位置・回転はGameObjectBaseのメソッドをオーバーライド） ---
    void SetPosition(const XMFLOAT3& pos) override;
    void SetRotation(const XMFLOAT3& rot) override;
    void SetViewMode(ViewMode mode) { viewMode = mode; }
    void SetGrounded(bool grounded) { isGrounded = grounded; }

    // --- カメラ関連 ---
    void SetCameraAngles(float yaw, float pitch);
    float GetCameraYaw() const;
    float GetCameraPitch() const;

    // --- 衝突処理 ---
    void ApplyPenetration(const XMFLOAT3& penetration);

    // --- GameObjectへのアクセス ---
    GameObject* GetGameObject();

    // --- Gunへのアクセス ---
    Gun* GetGun() { return m_gun.get(); }

    // --- ネットワーク用の強制位置設定 ---
    void ForceSetPosition(const XMFLOAT3& pos);
    void ForceSetRotation(const XMFLOAT3& rot);

    // --- HP関連 ---
    int GetHP() const { return hp; }
    int GetMaxHP() const { return MAX_HP; }
    bool IsAlive() const { return isAlive; }
    void TakeDamage(int dmg);
    void Respawn(const XMFLOAT3& spawnPoint);

    // --- ネットワーク同期用: 強制的に死亡状態にする ---
    void ForceDeath();

    // 水鉄砲
    WaterGun* GetWaterGun() { return m_waterGun.get(); }
};

} // namespace Game
