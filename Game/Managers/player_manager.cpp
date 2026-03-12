//==============================================================================
//  File   : player_manager.cpp
//  Brief  : プレイヤーマネージャー - プレイヤーの初期化・入力・更新処理
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "player_manager.h"
#include "bullet_manager.h"
#include "Game/Objects/bullet.h"
#include "Game/Objects/camera.h"
#include "Game/Map/map.h"
#include "Game/Map/map_renderer.h"
#include "Engine/Graphics/primitive.h"
#include "Engine/Graphics/texture_loader.h"
#include "Engine/Core/renderer.h"
#include "NetWork/network_manager.h"
#include "NetWork/network_common.h"
#include "Engine/Input/input_manager.h"
#include <cmath>
#include <algorithm>
#include <memory>

namespace Game {

//==========================================================
// PlayerManager 実装
//==========================================================

PlayerManager* PlayerManager::instance = nullptr;

//==========================================================
// コンストラクタ
//==========================================================
PlayerManager::PlayerManager()
    : activePlayerId(1)
    , player1Initialized(false)
    , player2Initialized(false)
    , initialPlayerLocked(false) {
}

//==========================================================
// シングルトンインスタンス取得
//==========================================================
PlayerManager& PlayerManager::GetInstance() {
    if (!instance) {
        instance = new PlayerManager();
    }
    return *instance;
}

//==========================================================
// 初期アクティブプレイヤー設定
//==========================================================
void PlayerManager::SetInitialActivePlayer(int playerId) {
    if (playerId == 1 || playerId == 2) {
        initialPlayerLocked = true;
        activePlayerId = playerId;
    }
}

//==========================================================
// 初期化処理
//==========================================================
void PlayerManager::Initialize(Map* map, ID3D11ShaderResourceView* texture) {
    // --- Playerにはtexture.jpgを全面貼り付け（一度だけロードしてキャッシュ） ---
    if (!m_playerTexture) {
        m_playerTexture = Engine::TextureLoader::Load(
            Engine::GetDevice(), L"resource/texture/texture.jpg");
        if (!m_playerTexture) {
            // テクスチャのロードに失敗した場合はフォールバック
            m_playerTexture = texture;
        }
    }

    // --- 常に両方のプレイヤーを初期化する ---
    if (!player1Initialized) {
        player1.Initialize(map, m_playerTexture, 1, ViewMode::FIRST_PERSON);
        player1.SetPosition(XMFLOAT3(0.0f, 3.0f, 0.0f));
        player1Initialized = true;
    }
    if (!player2Initialized) {
        player2.Initialize(map, m_playerTexture, 2, ViewMode::FIRST_PERSON);
        player2.SetPosition(XMFLOAT3(3.0f, 3.0f, 0.0f));
        player2Initialized = true;
    }
    // activePlayerIdはSetInitialActivePlayer()で設定済み
}

//==========================================================
// アクティブプレイヤー取得
//==========================================================
Player* PlayerManager::GetActivePlayer() {
    if (activePlayerId == 1 && player1Initialized) return &player1;
    if (activePlayerId == 2 && player2Initialized) return &player2;
    return nullptr;
}

//==========================================================
// プレイヤー取得（ID指定）
//==========================================================
Player* PlayerManager::GetPlayer(int playerId) {
    if (playerId == 1 && player1Initialized) return &player1;
    if (playerId == 2 && player2Initialized) return &player2;
    return nullptr;
}

//==========================================================
// 更新処理
//==========================================================
void PlayerManager::Update(float deltaTime) {
    HandleInput(deltaTime);
    if (player1Initialized) player1.Update(deltaTime);
    if (player2Initialized) player2.Update(deltaTime);
    BulletManager::GetInstance().Update(deltaTime);
}

//==========================================================
// 描画処理
//==========================================================
void PlayerManager::Draw() {
    if (player1Initialized) player1.Draw();
    if (player2Initialized) player2.Draw();
    BulletManager::GetInstance().Draw();
}

//==========================================================
// アクティブプレイヤー切り替え
//==========================================================
void PlayerManager::SetActivePlayer(int playerId) {
    if (initialPlayerLocked) return;

    if (playerId == 1 || playerId == 2) {
        Player* current = GetActivePlayer();
        CameraManager& camMgr = CameraManager::GetInstance();
        if (current) {
            current->SetCameraAngles(camMgr.GetRotation(), camMgr.GetPitch());
        }

        activePlayerId = playerId;

        Player* next = GetActivePlayer();
        if (next) {
            camMgr.SetRotation(next->GetCameraYaw());
            camMgr.SetPitch(next->GetCameraPitch());
            camMgr.UpdateCameraForPlayer(activePlayerId);
        }
    }
}

//==========================================================
// 入力処理
//==========================================================
void PlayerManager::HandleInput(float deltaTime) {
    Player* activePlayer = GetActivePlayer();
    if (!activePlayer) return;

    // InputManagerからコマンドを取得
    const InputCommand& cmd = InputManager::Cmd();

    // --- 移動入力 ---
    XMFLOAT3 moveDirection = { 0.0f, 0.0f, 0.0f };

    float yawRad = XMConvertToRadians(activePlayer->GetRotation().y);
    XMFLOAT3 forward = { sinf(yawRad), 0.0f, cosf(yawRad) };
    XMFLOAT3 right = { cosf(yawRad), 0.0f, -sinf(yawRad) };

    // InputManagerの抽象化された入力を使用
    // アナログ入力（ゲームパッドスティック or キーボードのデジタル値）
    float analogX = cmd.moveAnalogX;  // 左右: -1.0 ~ 1.0
    float analogY = cmd.moveAnalogY;  // 前後: -1.0 ~ 1.0 (負が前進)

    // アナログ値を移動方向に変換
    // moveAnalogY: 負=前進(W)、正=後退(S)
    // moveAnalogX: 負=左(A)、正=右(D)
    moveDirection.x = -forward.x * analogY + right.x * analogX;
    moveDirection.z = -forward.z * analogY + right.z * analogX;

    // 移動方向を正規化
    float moveLength = sqrtf(moveDirection.x * moveDirection.x + moveDirection.z * moveDirection.z);
    if (moveLength > 1.0f) {
        // 斜め移動時に速度が上がらないよう正規化（ただし1.0以下の場合はアナログ値を維持）
        moveDirection.x /= moveLength;
        moveDirection.z /= moveLength;
    }

    activePlayer->Move(moveDirection, deltaTime);

    // --- ジャンプ ---
    if (cmd.jumpTrigger) {
        activePlayer->Jump();
    }

    // --- 射撃（attackTrigger: 押した瞬間のみtrue） ---
    if (activePlayer->IsAlive() && cmd.attackTrigger) {
        // カメラの向きから発射方向を計算（pitch込みで上下にも飛ぶ）
        CameraManager& cam = CameraManager::GetInstance();
        float shootYaw = XMConvertToRadians(cam.GetRotation());
        float shootPitch = XMConvertToRadians(cam.GetPitch());

        XMFLOAT3 dir = {
            sinf(shootYaw) * cosf(shootPitch),
            sinf(shootPitch),
            cosf(shootYaw) * cosf(shootPitch)
        };

        // 発射位置: プレイヤーの目の高さ + 前方に少しオフセット
        XMFLOAT3 pos = activePlayer->GetPosition();
        pos.y += 1.0f;
        pos.x += dir.x * 0.6f;
        pos.y += dir.y * 0.6f;
        pos.z += dir.z * 0.6f;

        // 弾を生成（撃ったプレイヤーのIDを渡して自弾判定に使う）
        auto b = std::make_unique<Bullet>();
        b->Initialize(GetPolygonTexture(), pos, dir, activePlayer->GetPlayerId());
        BulletManager::GetInstance().Add(std::move(b));

        // ネットワーク接続中なら弾の発射情報を送信
        if (g_network.is_host() || g_network.getMyPlayerId() != 0) {
            PacketBullet pb = {};
            pb.type = PKT_BULLET;
            pb.seq = 0;
            pb.ownerPlayerId = (uint32_t)activePlayer->GetPlayerId();
            pb.posX = pos.x; pb.posY = pos.y; pb.posZ = pos.z;
            pb.dirX = dir.x; pb.dirY = dir.y; pb.dirZ = dir.z;
            g_network.send_bullet(pb);
        }
    }

    // --- リスポーン ---
    if (cmd.reloadTrigger) {
        activePlayer->Respawn({ 0.0f, 3.0f, 0.0f });

        // リスポーン位置を即座にGameObjectにも反映
        GameObject* go = activePlayer->GetGameObject();
        if (go) {
            go->setPosition({ 0.0f, 3.0f, 0.0f });
            go->setRotation({ 0.0f, 0.0f, 0.0f });
        }
    }
}

//==========================================================
// グローバル関数ラッパー
//==========================================================

void InitializePlayers(Map* map, ID3D11ShaderResourceView* texture) {
    PlayerManager::GetInstance().Initialize(map, texture);
}

void UpdatePlayers() {
    constexpr float fixedDelta = 1.0f / 60.0f;
    PlayerManager::GetInstance().Update(fixedDelta);
}

void DrawPlayers() {
    PlayerManager::GetInstance().Draw();
}

GameObject* GetActivePlayerGameObject() {
    Player* activePlayer = PlayerManager::GetInstance().GetActivePlayer();
    return activePlayer ? activePlayer->GetGameObject() : nullptr;
}

Player* GetActivePlayer() {
    return PlayerManager::GetInstance().GetActivePlayer();
}

//==========================================================
// ネットワーク同期: 相手プレイヤーの状態を強制更新
//==========================================================
void PlayerManager::ForceUpdatePlayer(int playerId, const XMFLOAT3& pos, const XMFLOAT3& rot, bool alive) {
    Player* p = GetPlayer(playerId);
    if (!p) return;

    // 生存状態が変化した時のみ処理
    bool currentlyAlive = p->IsAlive();

    if (alive && !currentlyAlive) {
        // 死亡→生存: リスポーン
        p->Respawn(pos);
    } else if (!alive && currentlyAlive) {
        // 生存→死亡: 強制死亡
        p->ForceDeath();
    }

    // 生存中のみ位置を更新
    if (p->IsAlive()) {
        p->ForceSetPosition(pos);
        p->ForceSetRotation(rot);
    }
}

} // namespace Game
